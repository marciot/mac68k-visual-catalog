#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SIOUX.h>

#include <Files.h>

#include "FSUtils.h"
#include "IconUtils.h"
#include "pngout.h"
#include "DesktopDB.h"
#include "MacRoman.h"

#include <set.h>

#define VERBOSE 0
#define DO_NOT_LOG_EMPTY_ENTITIES 1

extern "C" {
  #include "sha1.h"
  #include "base64.h"
}

enum {
  kCustomIconID = -16455
};

#include "miniz.h"
extern tdefl_status status;

// This will write out an icon, if the file already exists, this will return dupFNErr
static OSErr writeIcon (FSSpec fsSpec, Icon &icon, char *utf8_info) {
  short fRefNum;
  struct pngout s;
  long n;

  OSErr err = HCreate (fsSpec.vRefNum, fsSpec.parID, fsSpec.name, 'PICT', 'PNGf');
  if (err) return err;

  err = HOpenDF (fsSpec.vRefNum, fsSpec.parID, fsSpec.name, fsWrPerm, &fRefNum);
  if (err) return err;

  pngout_start(&s, 32, 32, icon.isMono);

  while (pngout_has_data(&s, (size_t*)&n)) {
    err = FSWrite(fRefNum, &n, s.output);
    if (err) return err;
  }

  pngout_transparent_rgb(&s, 0x12, 0x34, 0x56);

  while (pngout_has_data(&s, (size_t*)&n)) {
    err = FSWrite(fRefNum, &n, s.output);
    if (err) return err;
  }

  if (utf8_info && utf8_info[0]) {
    pngout_utf8_text(&s, "MacOS Info", utf8_info);

    while (pngout_has_data(&s, (size_t*)&n)) {
      err = FSWrite(fRefNum, &n, s.output);
      if (err) return err;
    }
  }

  IconIterator iter(icon);
  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 32; x++) {
      RGBColor rgb;
      iter.getNextPixel(rgb);
      pngout_rgb (&s, rgb.red >> 8, rgb.green >> 8, rgb.blue >> 8);

      while (pngout_has_data(&s, (size_t*)&n)) {
        err = FSWrite(fRefNum, &n, s.output);
        if (err) return err;
      }
    }
  }

  pngout_end(&s);
  while (pngout_has_data(&s, (size_t*)&n)) {
    err = FSWrite(fRefNum, &n, s.output);
    if (err) return err;
  }

  if (status != TDEFL_STATUS_DONE) {
    printf("Failed to write out complete compressed data for %#s\n", fsSpec.name);
  }

  #if VERBOSE

    err = GetFPos(fRefNum, &n);
    printf("Wrote \"%#s\" (%ld bytes)\n", fsSpec.name, n);
  #endif

  FSClose(fRefNum);
  return err;
}

static void setHashedNameExtension (Str31 name, char *extension) {
  name[0]  =  31;
  name[28] = '.';
  name[29] = extension[0];
  name[30] = extension[1];
  name[31] = extension[2];
}

static void getHashedName (Icon &icon, Str31 name) {
  uint8_t sha_key[SHA1HashSize];

  SHA1Context sha;
  SHA1Reset(&sha);

  IconIterator iter(icon);
  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 32; x++) {
      RGBColor rgb;
      iter.getNextPixel(rgb);
      SHA1Input(&sha, (uint8_t*)&rgb,   sizeof(rgb));
    }
  }

  SHA1Result(&sha, sha_key);

  // This will writes the hash as a C string, but leave out a byte
  // to make this into a pascal string

  base64_encode(sha_key, SHA1HashSize, (char*) name + 1, NULL);

  // Replace invalid characters
  replaceChars((char*) name + 1, '/', '_');
  replaceChars((char*) name + 1, '+', '-');

  // "name" will consists of 28 characters, with the last being an '='
  // When we add the extension, we overwrite the equals to have a
  // string of 30 characters, which is one less the exact maximum
  // length for a Macintosh filename, null terminate it for 32 chars.

  setHashedNameExtension (name, "png");
}

struct IconInfo;

static class IconDatabase {
  public:
    FSSpec appDir;
    FSSpec dbDir;

    OSErr open() {
      // Find the current directory
      OSErr err = HGetVol (appDir.name, &appDir.vRefNum, &appDir.parID);
      if (err) return err;

      // Create the database directory
      return FSpSubDirCreate (&appDir, &dbDir, "\pIcon Database");
    }

    // Writes an UTF8 info string consisting of the file type followed
    // by the creator. May occupy up to 26 bytes in str.
    void getInfoString (OSType fileType, OSType fileCreator, char* str) {
      char src[10];
      BlockMove (&fileType, src, 4);
      BlockMove (&fileCreator, src + 5, 4);
      src[4] = ' ';
      src[9] = 0;
      toUTF8Char(macRoman, src, str);
    }

    OSErr getDirForHash (FSSpec *fsSpec, Str31 name) {
      Str31 prefix;
      prefix[0] = 2;
      prefix[1] = name[1];
      prefix[2] = name[2];
      OSErr err = FSpSubDirCreate (&dbDir, fsSpec, prefix);
      BlockMove (name, fsSpec->name, name[0] + 1);
      return err;
    }

    OSErr getDirForApp (FSSpec *fsSpec, OSType creator, Str31 name) {
      char src[5], filename[4*5+2];
      BlockMove (&creator, src, 4);
      src[4] = 0;

      toFilename(macRoman, src, filename, '#');

      Str63 prefix;
      prefix[0] = strlen(filename);
      BlockMove (filename, prefix + 1, prefix[0]);

      OSErr err = FSpSubDirCreate (&dbDir, fsSpec, prefix);
      BlockMove (name, fsSpec->name, name[0] + 1);
      return err;
    }

    OSErr addIcon(IconInfo *icnInfo, Icon &iconIterator);
}; // IconDatabase

typedef enum {
  kUserIcon = 0,
  kFinderBundleIcon = 1,
  kFinderCustomIcon = 2
} IconSource;

struct Ascending {
  bool operator()(short a, short b) const {
    return a < b;
  }
};

struct IconInfo {
  IconDatabase db;

  IconSource source;

  // Finder info, if source = kFinderBundleIcon
  OSType bndlType;
  OSType bndlCreator;

  set<short, Ascending> bndlFamilyIDs;

  Boolean dryRun;

  unsigned short dirDepth;

  unsigned long iconsUnique;
  unsigned long iconsFound;
  unsigned long iconsFromBundle;
  unsigned long iconsCustom;
  unsigned long iconsUpgraded;
  unsigned long searchedFiles;
  unsigned long searchedDirs;

  Str255 lastSeenDirName;
  long lastUpdate;

  FILE *log;

  IconInfo() : iconsUnique(0), iconsFound(0), iconsFromBundle(0), iconsCustom(0), iconsUpgraded(0),
               searchedFiles(0), searchedDirs(0), dirDepth(0), dryRun(0), dirDepth(0), lastUpdate(TickCount()), log(0) {
  }

  void showProgress() {
      if (TickCount() - lastUpdate > 240) {
        lastUpdate = TickCount();
        printf("Searching... %ld files (free mem: %ld)\n", searchedFiles, FreeMem());
      }
  }

  void printSummary() {
    printf("\n");
    printf("Found Icons:       %ld\n", iconsFound);
    printf("    Bundle Icons:  %ld\n", iconsFromBundle);
    printf("    Custom Icons:  %ld\n", iconsCustom);
    printf("  Upgraded Icons:  %ld\n", iconsUpgraded);
    printf("Unique Icons:      %ld\n", iconsUnique);
    printf("Folders Searched:  %ld\n", searchedDirs);
    printf("Files Searched:    %ld\n", searchedFiles);
  }

  char getIconTypeChar() {
    char typeChar = 0;
    switch (source) {
      case kFinderBundleIcon:
        typeChar = 'd';
        if (bndlType == 'APPL') typeChar = 'a';
        if (bndlType == 'cdev') typeChar = 'c';
        if (bndlType == 'sdev') typeChar = 'e';
        if (bndlType == 'INIT') typeChar = 'e';
        break;
      case kFinderCustomIcon:
        typeChar = 'u';
        break;
    }
    return typeChar;
  }
};

typedef enum {
  kLogStart = 0,
  kLogDir  = 1,
  kLogFile = 2,
  kLogIcon = 3,
  kLogEnd = 4
} LogType;

static void writeToLog(IconInfo *icnInfo, StringPtr name, LogType type) {
  if (icnInfo->log) {
    const char iconTypeChar = icnInfo->getIconTypeChar();
    char *outputFormat;
    switch (type) {
      case kLogEnd:
        outputFormat = "EOF\n";
        break;
      case kLogIcon:
        outputFormat = iconTypeChar ?
          "%*s %#.27s:%c\n" :
          "%*s %#.27s\n";
        break;
      default:
        outputFormat = "%*s:%#s\n";
    }
    fprintf(icnInfo->log, outputFormat, icnInfo->dirDepth, "", name, iconTypeChar);
  }
}

OSErr IconDatabase::addIcon (IconInfo *icnInfo, Icon &icon) {
  Str31 name;
  getHashedName (icon, name);

  writeToLog (icnInfo, name, kLogIcon);

  OSErr err = noErr;
  if (!icnInfo->dryRun) {
    FSSpec fsSpec;
    err = getDirForHash (&fsSpec, name);
    if (err == noErr) {
      FSSpec upperCaseSpec = fsSpec;
      setHashedNameExtension (upperCaseSpec.name, "PNG");

      Boolean existsAsMixedCase;
      if (FSpFileExistsCaseSensitive(&upperCaseSpec, &existsAsMixedCase)) {
        // If a hashed file exists with an UPPERCASE extension,
        // this came from a BNDL and takes priority over others.
        err = dupFNErr;
      } else {
        char utf8_info[26];
        utf8_info[0] = 0;

        switch (icnInfo->source) {
          case kFinderBundleIcon:
            if (existsAsMixedCase) {
              OSErr err = FSpDelete(&fsSpec);     // Delete file with lowercase extension
              if(err != noErr) printf("File deletion failed\n");
              icnInfo->iconsUpgraded++;
            }
            icnInfo->iconsFromBundle++;
            getInfoString(icnInfo->bndlType, icnInfo->bndlCreator, utf8_info);
            fsSpec = upperCaseSpec; // Use UPPERCASE name for icons from BNDL.
            break;
          case kFinderCustomIcon:
            icnInfo->iconsCustom++;
            break;
        }
        err = writeIcon (fsSpec, icon, utf8_info);
      } // !uppercaseExists
    }
  } // !dryRun

  switch (err) {
    case noErr:    icnInfo->iconsUnique++; // Intentional fall-thru
    case dupFNErr: icnInfo->iconsFound++; break;
  }

  return err;
}

static OSErr readCIconResource (IconInfo *icnInfo, CIconPtr icon, short hSize) {
  OSErr err = noErr;
  const short rowBytes  = icon->iconPMap.rowBytes & 0x7FFF;
  const short pixelSize = icon->iconPMap.pixelSize;
  const short height    = icon->iconPMap.bounds.bottom
                        - icon->iconPMap.bounds.top;
  const short width     = icon->iconPMap.bounds.right
                        - icon->iconPMap.bounds.left;
  Ptr maskPtr =     (Ptr)&icon->iconMaskData;
  Ptr bitsPtr = maskPtr + icon->iconMask.rowBytes * height;
  Ptr ctabPtr = bitsPtr + icon->iconBMap.rowBytes * height;
  Ptr cpixPtr = ctabPtr + 8 + (((CTabPtr)ctabPtr)->ctSize+1)*sizeof(ColorSpec);
  Ptr end     = cpixPtr + rowBytes * height;
  if (hSize == (end - (Ptr)icon)) {
    if ((height == 32) && (width == 32)) {
      if (bitsPtr != ctabPtr) {
        err = icnInfo->db.addIcon (icnInfo, Icon(bitsPtr, maskPtr, 1, NULL));
      }
      if (cpixPtr != end) {
        err = icnInfo->db.addIcon (icnInfo, Icon(cpixPtr, maskPtr, pixelSize, (CTabPtr)ctabPtr));
      }
    } else {
      #if VERBOSE
        printf("Unsupported height for cicn %d x %d\n", width, height);
      #endif
      err = paramErr;
    }
  } else {
    #if VERBOSE
        printf("Size mismatch! cicn %ld != %ld\n", hSize, (end - (Ptr)icon));
    #endif
    err = paramErr;
  }
  return err;
}

static OSErr readIconResource (IconInfo *icnInfo, Ptr icon, short hSize) {
  OSErr err = noErr;
  if ((hSize != 128) && (hSize != 256)) {
    #if VERBOSE
      printf("Size mismatch! ICON %ld\n", hSize);
    #endif
    err = paramErr;
  } else {
    if (hSize == 128) {
      err = icnInfo->db.addIcon (icnInfo, Icon(icon, NULL, 1, NULL));
    } else {
      // This is an ICON likely formatted as an ICN#
      err = icnInfo->db.addIcon (icnInfo, Icon(icon, icon + 128, 1, NULL));
    }
  }
  return err;
}

static OSErr readIconListResource (IconInfo *icnInfo, Ptr icon, short hSize) {
  if (icon == NULL) {
    #if VERBOSE
      printf("Got NULL value in readIconListResource\n");
    #endif
    return paramErr;
  }
  OSErr err = noErr;
  if (hSize == 128) {
    // This is an ICN# likely formatted as an ICON
    err = icnInfo->db.addIcon (icnInfo, Icon(icon, NULL, 1, NULL));
  }
  else if ((hSize % 256) != 0) {
    #if VERBOSE
      printf("Size mismatch! ICN# %ld\n", hSize);
    #endif
    err = paramErr;
  }
  else {
    short nIcons = hSize / 256;

    // If nIcons > 1, this is an "unstandard" ICN# with extra entries.

    Ptr p = icon;
    for (int i = 0; i < nIcons; i++) {
      err = icnInfo->db.addIcon (icnInfo, Icon(p, p + 128, 1, NULL));
      p += 256;
    }
  }
  return err;
}

static OSErr readColorIconListResource (IconInfo *icnInfo, Ptr cIcon, Ptr icon, short hSize, short depth) {
  if (cIcon == NULL || icon == NULL) {
    #if VERBOSE
      printf("Got NULL value in readColorIconListResource\n");
    #endif
    return paramErr;
  }
  OSErr err = noErr;
  if (hSize != (128 * depth)) {
    #if VERBOSE
      printf("Size mismatch! %ld\n", hSize);
    #endif
    err = paramErr;
  } else {
    err = icnInfo->db.addIcon (icnInfo, Icon(cIcon, icon + 128, depth, NULL));
  }
  return err;
}

static OSErr readIconFamily(IconInfo *icnInfo, Handle h, short hSize, short appResFile, short refResFile) {
  short theID;
  ResType theType;
  Str255 name;
  GetResInfo (h, &theID, &theType, name);

  // Each icon family is visited twice, when processing BNDL resources
  // and again when processing ICN# resources. Avoid reprocessing..

  if (icnInfo->bndlFamilyIDs.find(theID) == icnInfo->bndlFamilyIDs.end()) {
    // Mark icon family as processed...
    icnInfo->bndlFamilyIDs.insert(theID);
  } else {
    // ...don't process the second time
    return dupFNErr;
  }

  if (theID == kCustomIconID) {
    icnInfo->source = kFinderCustomIcon;
  }

  OSErr err = readIconListResource (icnInfo, *h, hSize);

  // Read related color icons, as they depend on the mask present in the 'ICN#'
  if((err == noErr) || (err == dupFNErr)) {
    UseResFile(refResFile);
    Handle h8 = Get1Resource ('icl8', theID);
    Handle h4 = Get1Resource ('icl4', theID);
    UseResFile(appResFile);

    if (h8) {
      HLock(h8);
      err = readColorIconListResource (icnInfo, *h8, *h, GetHandleSize(h8), 8);
      HUnlock(h8);
      ReleaseResource(h8);
    }

    if (h4) {
      HLock(h4);
      err = readColorIconListResource (icnInfo, *h4, *h, GetHandleSize(h4), 4);
      HUnlock(h4);
      ReleaseResource(h4);
    }
  }
  return err;
}

static OSErr readFinderBundle (IconInfo *icnInfo, Ptr data, short size, short appResFile, short refResFile) {
  if (data == NULL) {
    #if VERBOSE
      printf("Got NULL value in readFinderBundle\n");
    #endif
    return paramErr;
  }
  typedef struct {
    OSType signature;
    short verResId;
    short numResTypes;
  } BundleRec, *BundlePtr;

  typedef struct {
    OSType resType;
    short numResources;
  } BundleResTypeRec, *BundleResTypePtr;

  typedef struct {
    short localID;
    short resID;
  } BundleResRec, *BundleResPtr;

  typedef struct {
    OSType fileType;
    short localIconID;
  } *FileRefPtr, **FileRefHandle;

  BundlePtr bundleRec = (BundlePtr)data;
  data += sizeof(BundleRec);

  BundleResRec *frefList;
  BundleResRec *iconList;
  short frefCount = 0;
  short iconCount = 0;

  if (size < sizeof(BundleRec)) {
    printf("Incorrect BNDL size\n");
    return paramErr;
  }

  for (int i = 0; i <= bundleRec->numResTypes; i++) {
    BundleResTypePtr resTypeRec = (BundleResTypePtr)data;
    data += sizeof(BundleResTypeRec);

    switch (resTypeRec->resType) {
      case 'FREF': frefList = (BundleResPtr)data; frefCount = resTypeRec->numResources + 1; break;
      case 'ICN#': iconList = (BundleResPtr)data; iconCount = resTypeRec->numResources + 1; break;
      case 'cicn':
      case 'icl8':
      case 'icl4':
      case 'ics8':
      case 'ics4':
      case 'ics#':
        // Generally apps only have the 'ICN#' in the 'BNDL' and then all other icons
        // in the icon family share an ID. This is how ResEdit treats the BNDL so we
        // assume the same and ignore other icon types.
        #if VERBOSE
          printf("Ignorning unexpected icon type in BNDL: %.4s:\n", &resTypeRec->resType);
        #endif
        break;
      default:
        printf("Unknown resource type in BNDL: %.4s:\n", &resTypeRec->resType);
    }
    data += sizeof(BundleResRec) * (resTypeRec->numResources + 1);
  }

  // Go through all the FREF resources in the BNDL

  OSErr err = noErr;

  for (int i = 0; i < frefCount; i++) {
    UseResFile(refResFile);
    FileRefHandle h = (FileRefHandle) Get1Resource ('FREF', frefList[i].resID);
    UseResFile(appResFile);
    if (h) {
      // Read the FREF resource
      HLock ((Handle)h);
      OSType fileType = (*h)->fileType;
      const short localIconID = (*h)->localIconID;
      HUnlock ((Handle)h);
      ReleaseResource((Handle)h);

      if (localIconID < iconCount) {
        const short iconFamilyID = iconList[localIconID].resID;

        #if VERBOSE
          printf("  FREF %d -> ICN# %d\n", frefList[i].resID, iconFamilyID);
        #endif

        // Read the icon family
        UseResFile(refResFile);
        Handle icn = Get1Resource ('ICN#', iconFamilyID);
        UseResFile(appResFile);
        if (icn) {
          icnInfo->source = kFinderBundleIcon;
          icnInfo->bndlType = fileType;
          icnInfo->bndlCreator = bundleRec->signature;

          HLock(icn);
          err = readIconFamily(icnInfo, icn, GetHandleSize(icn), appResFile, refResFile);
          HUnlock(icn);
          ReleaseResource(icn);

          if (err && (err != dupFNErr) && (err != paramErr)) {
            break;
          }
        } // icn
      } // (*h)->localIconID < iconCount
    } // h
  } // for(...)

  return err;
}

static OSErr searchCurResForIcons(short appResFile, short refResFile, IconInfo *icnInfo, OSType iconType) {
  OSErr err = noErr;
  UseResFile(refResFile);
  short numResources = Count1Resources (iconType);
  UseResFile(appResFile);

  for (int i = 1; i <= numResources; i++) {
    UseResFile(refResFile);
    Handle h = Get1IndResource (iconType, i);
    UseResFile(appResFile);

    icnInfo->source = kUserIcon;

    if (h) {
      const Size hSize = GetHandleSize(h);
      HLock(h);
      switch (iconType) {
        case 'BNDL': err = readFinderBundle (icnInfo, *h, hSize, appResFile, refResFile); break;
        case 'cicn': err = readCIconResource (icnInfo, *(CIconHandle)h, hSize); break;
        case 'ICON': err = readIconResource (icnInfo, *h, hSize); break;
        case 'ICN#': err = readIconFamily (icnInfo, h, hSize, appResFile, refResFile); break;
      }
      HUnlock(h);
      ReleaseResource(h);
    } // h != NULL
    if (err && (err != dupFNErr) && (err != paramErr)) {
      return err;
    }
  } // for (...)
  if (err && (err != dupFNErr) && (err != paramErr)) {
    return err;
  }
  return noErr;
}

static OSErr searchResFileForIcons(IconInfo *icnInfo, short vRefNum, long parID, StringPtr name) {
  OSErr err;
  short appResFile = CurResFile();
  SetResLoad(false);
  short resRefFile = HOpenResFile(vRefNum, parID, name, fsRdPerm);
  UseResFile(appResFile);
  SetResLoad(true);
  if (resRefFile != -1) {

    icnInfo->bndlFamilyIDs.erase(icnInfo->bndlFamilyIDs.begin(), icnInfo->bndlFamilyIDs.end());

    if((err = searchCurResForIcons (appResFile, resRefFile, icnInfo, 'BNDL')) == noErr)
    if((err = searchCurResForIcons (appResFile, resRefFile, icnInfo, 'ICON')) == noErr)
    if((err = searchCurResForIcons (appResFile, resRefFile, icnInfo, 'cicn')) == noErr)
        err = searchCurResForIcons (appResFile, resRefFile, icnInfo, 'ICN#');

    if (resRefFile != appResFile) {
      CloseResFile(resRefFile);
      UseResFile(appResFile);
    }
  } else {
    err = ResError();
  }
  if (err) {
    printf("Error processing resource file, %#s %d\n", name, err);
  }
  return noErr;
}

static OSErr recursiveSearch(IconInfo *icnInfo, short vRefNum, long dirID = fsRtDirID) {
  CInfoPBRec pb;
  Str255 name;
  OSErr err;
  short index;

  for (index = 1;; index++) {
    memset(&pb, 0, sizeof(pb));

    pb.hFileInfo.ioNamePtr   = name;
    pb.hFileInfo.ioVRefNum   = vRefNum;
    pb.hFileInfo.ioDirID     = dirID;
    pb.hFileInfo.ioFDirIndex = index;

    err = PBGetCatInfoSync(&pb);

    if (err == fnfErr) return noErr;
    if (err != noErr)  return err;

    #if DO_NOT_LOG_EMPTY_ENTITIES
      const unsigned long prevIconsFound = icnInfo->iconsFound;
      fpos_t prevLogPos;
      fgetpos(icnInfo->log, &prevLogPos);
    #endif

    if (pb.hFileInfo.ioFlAttrib & ioDirMask) {
      if (pb.dirInfo.ioDrDirID == icnInfo->db.dbDir.parID) {
        continue; // Don't descend into directory we are creating
      }

      BlockMove(name, icnInfo->lastSeenDirName, name[0] + 1);
      writeToLog (icnInfo, pb.dirInfo.ioNamePtr, kLogDir);

      icnInfo->dirDepth++;
      err = recursiveSearch (icnInfo, vRefNum, pb.dirInfo.ioDrDirID);
      icnInfo->dirDepth--;
      icnInfo->searchedDirs++;
    } else {
      //Boolean hasCustomIcon   = pb.hFileInfo.ioFlFndrInfo.fdFlags & hasCustomIcon;
      //Boolean hasBundle       = pb.hFileInfo.ioFlFndrInfo.fdFlags & hasBundle;
      //Boolean hasBeenInited   = pb.hFileInfo.ioFlFndrInfo.fdFlags & hasBeenInited;
      Boolean hasResourceFork = pb.hFileInfo.ioFlRLgLen;
      if (hasResourceFork) {
        const isCustomDirIcon = (memcmp(pb.hFileInfo.ioNamePtr, "\pIcon\r", 6) == 0);
        const StringPtr logName = isCustomDirIcon ? icnInfo->lastSeenDirName : pb.hFileInfo.ioNamePtr;
        // When a directory has custom icon, log parent directory name instead
        writeToLog (icnInfo, logName, kLogFile);

        // Search the file for icons
        err = searchResFileForIcons(icnInfo, pb.hFileInfo.ioVRefNum, pb.hFileInfo.ioFlParID, pb.hFileInfo.ioNamePtr);
      }
      icnInfo->searchedFiles++;
      icnInfo->showProgress();
    }

    #if DO_NOT_LOG_EMPTY_ENTITIES
      if (icnInfo->iconsFound == prevIconsFound) {
        fsetpos(icnInfo->log, &prevLogPos);
      }
    #endif
  }
  return err;
}

static OSErr databaseSearch(IconInfo *icnInfo, short vRefNum, long dirID = fsRtDirID) {

  short refNumDTDF;
  OSErr err = HOpenDF (vRefNum, 0, "\p:Desktop DF", fsRdPerm, &refNumDTDF);
  if (err) return err;

  try {
    OSType maskCreator = 0, maskFileType = 0;
    char iconAndMask[256];
    char colorIcon[1024];

    DTDBHandle dtdb = LoadVolumeDesktopDB (vRefNum);
    DTDBIterator iter(dtdb);

    DTDBRec *recPtr;
    short recLen;

    while (iter.getNextRecord (&recPtr, &recLen)) {
      if (recPtr->kind == kApplRecKind) {
        printf("%3d : %.4s  : %#s\n", recPtr->applRec.idx, &recPtr->applRec.creator, recPtr->applRec.name);
      }
      else if (recPtr->kind == kIconRecKind) {

        icnInfo->showProgress();

        if (recPtr->iconRec.fileType == 'paul') {
          continue;
        }

        long readLen;
        Ptr readPtr;
        short depth;

        switch (recPtr->iconRec.iconType) {
          case k1BitLargeIconAndMask:
            maskCreator  = recPtr->iconRec.creator;
            maskFileType = recPtr->iconRec.fileType;
            readLen = 256;
            readPtr = iconAndMask;
            depth = 1;
            break;
          case k4BitLargeIcon:
            readLen = 512;
            readPtr = colorIcon;
            depth = 4;
            break;
          case k8BitLargeIcon:
            readLen = 1024;
            readPtr = colorIcon;
            depth = 8;
            break;
          default:
            continue;
        }

        if ((maskCreator  != recPtr->iconRec.creator) ||
            (maskFileType != recPtr->iconRec.fileType)) {
          printf("Encountered color icon not preceeded by mask. Unable to write\n");
          continue;
        }

        if (recPtr->iconRec.dfLength != readLen) {
          printf("Length mismatch\n");
          continue;
        }

        err = SetFPos (refNumDTDF, fsFromStart, recPtr->iconRec.dfOffset);
        err = FSRead(refNumDTDF, &readLen, readPtr);
        if (recPtr->iconRec.dfLength == readLen) {
          icnInfo->source      = kFinderBundleIcon;
          icnInfo->bndlType    = recPtr->iconRec.fileType;
          icnInfo->bndlCreator = recPtr->iconRec.creator;
          icnInfo->db.addIcon (icnInfo, Icon(readPtr, iconAndMask + 128, depth, NULL));
        } else {
          printf("Expected to read %d, got %ld\n", recPtr->iconRec.dfLength, readLen);
        }
      }
    }
  }
  catch (OSErr _err) {
    if (_err == memFullErr) {
      printf("Not enough memory to load \"Desktop DB\" into memory!\n");
    }
    err = _err;
  }

  FSClose (refNumDTDF);
  return err;
}

static short getChoice (char *prompt) {
  short selection;

  do {
    printf(prompt);
    if (scanf("%d", &selection) != 1) {
      printf("Invalid input. Please enter a valid integer.\n");
      continue;
    }
  } while (0);
  return selection;
}

void main(void) {
  IconInfo i;

  FSSpec spec;

  SIOUXSettings.autocloseonquit = FALSE;
  SIOUXSettings.asktosaveonclose = FALSE;

  initUnicodeTables();
  if(!ColorMap::loadDefaultTables()) {
    printf("Failed to load color maps\n");
    return;
  }

  if(!initCompressor()) {
    printf("Failed to initialize compressor\n");
    return;
  }

  // List the volumes

  long freeBytes;
  printf("\n");
  printf("\nVolume List\n");
  printf("Num| Name                           | vRefNum\n");
  printf("---|--------------------------------|--------\n");
  for(int drvNum = 1; drvNum < 100; drvNum++) {
    OSErr err = GetVInfo (drvNum, spec.name, &spec.vRefNum, &freeBytes);
    switch (err) {
      case noErr:
        printf("%2d | %#-31s | %3d\n", drvNum, spec.name, spec.vRefNum);
        break;
      case nsvErr:
        break;
      default:
        drvNum = 100;
    }
  }

  const selectedDriveNum = getChoice ("\nPlease select a drive number: ");

  short runMode = 0;

  do {
    printf("\n");
    printf("\nSearch Type\n");
    printf("   1) Deep: Search all files for application, document and custom dialog icons.\n");
    printf("   2) Shallow: Extract application and document icons from Finder database\n");

    printf("\n");
    printf("\nOther Options\n");
    printf("   3) Dry run (only log): %s\n", i.dryRun ? "yes" : "no");

    runMode = getChoice ("\nPlease select choice: ");

    if (runMode == 3) {
      i.dryRun = !i.dryRun;
    }
  } while (runMode != 1 && runMode != 2);


  // Get the volume reference number from the user selection

  OSErr err = GetVInfo (selectedDriveNum, spec.name, &spec.vRefNum, &freeBytes);

  char logFileName[32];
  sprintf(logFileName, "%#s.txt", spec.name);
  i.log = fopen(logFileName, "w");

  writeToLog(&i, spec.name, kLogStart);
  i.dirDepth++;

  printf("Starting search\n");

  i.db.open();

#if 0
  searchResFileForIcons(&i, spec.vRefNum, fsRtDirID, "\p:IconToPng:Audacity");
#else
  if (runMode == 1) {
    recursiveSearch (&i, spec.vRefNum);
  } else {
    databaseSearch (&i, spec.vRefNum);
  }
#endif

  i.printSummary();

  writeToLog(&i, NULL, kLogEnd);

  fclose(i.log);

  ColorMap::disposeDefaultTables();
}