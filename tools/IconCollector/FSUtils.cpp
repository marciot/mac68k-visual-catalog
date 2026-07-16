#include <string.h>

#include "FSUtils.h"

void replaceChars(char *str, char find, char replace) {
  if (str == NULL) return;
  for (
    char *current_pos = str;
    (current_pos = strchr(current_pos, find)) != NULL;
    *current_pos = replace
  );
}

// Creates a new directory within parentDir with the name. On exit, createdDir will be
// updated to the newly created directory. parentDir and createdDir may be identical.

OSErr FSpSubDirCreate (const FSSpecPtr parentDir, FSSpecPtr createdDir, Str63 name, ScriptCode scriptTag) {
  FSSpec newDir = *parentDir;
  BlockMove (name, newDir.name, sizeof(Str63));

  long createdDirID = 0;
  OSErr err = FSpDirCreate (&newDir, scriptTag, &createdDirID);
  if (err == dupFNErr) {
    CInfoPBRec pb;

    memset (&pb, 0, sizeof(pb));

    pb.dirInfo.ioNamePtr = newDir.name;
    pb.dirInfo.ioVRefNum = newDir.vRefNum;
    pb.dirInfo.ioDrDirID = newDir.parID;
    pb.dirInfo.ioFDirIndex = 0; // use parent ID + name lookup

    err = PBGetCatInfoSync(&pb);
    if (err == noErr) {
      if (pb.dirInfo.ioFlAttrib & ioDirMask) {
        createdDirID = pb.dirInfo.ioDrDirID;
      } else {
        return dupFNErr;
      }
    }
  }
  newDir.parID = createdDirID;
  *createdDir = newDir;
  return err;
}

Boolean FSpFileExists (const FSSpecPtr fsSpec) {
  CInfoPBRec pb;

  memset (&pb, 0, sizeof(pb));

  pb.dirInfo.ioNamePtr = fsSpec->name;
  pb.dirInfo.ioVRefNum = fsSpec->vRefNum;
  pb.dirInfo.ioDrDirID = fsSpec->parID;
  pb.dirInfo.ioFDirIndex = 0; // use parent ID + name lookup

  OSErr err = PBGetCatInfoSync(&pb);
  if (err == noErr) {
    if (pb.dirInfo.ioFlAttrib & ioDirMask) {
      return false;
    } else {
      return true;
    }
  }
  return false;
}

Boolean FSpFileExistsCaseSensitive (const FSSpecPtr fsSpec, Boolean *caseInsensitiveResult) {
  // First do a case insensitive search, return false if nothing found

  const Boolean matchCaseInsensitive = FSpFileExists (fsSpec);
  if (caseInsensitiveResult) *caseInsensitiveResult = matchCaseInsensitive;
  if (!matchCaseInsensitive) {
    return false;
  }

  // Now do a search to find the file's actual capitalization.

  CInfoPBRec pb;
  Str63 name;

  memset (&pb, 0, sizeof(pb));

  for (short index = 1;; index++) {
    pb.dirInfo.ioNamePtr = name;
    pb.dirInfo.ioVRefNum = fsSpec->vRefNum;
    pb.dirInfo.ioDrDirID = fsSpec->parID;
    pb.dirInfo.ioFDirIndex = index;

    OSErr err = PBGetCatInfoSync(&pb);
    if (err == noErr) {
      if (!(pb.dirInfo.ioFlAttrib & ioDirMask)) {
        if (memcmp(name, fsSpec->name, fsSpec->name[0]) == 0) {
          return true;
        }
      }
    } else {
      break;
    }
  }
  return false;
}