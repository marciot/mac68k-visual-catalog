#pragma once

#include <Types.h>
#include <Memory.h>

const long NodeSize = 512;

enum {
  kBTreeInvalidErr = -26200,
  kMaxBTreeDepth   = 10
};

// BTNodeDescriptor.kind:

enum {
  kBTIndexNode     = 0,
  kBTHeaderNode    = 1,
  kBTMapNode       = 2,
  kBTLeafNode      = -1
};

#pragma options align=mac68k

// https://developer.apple.com/library/archive/technotes/tn/tn1150.html#BTrees

struct BTNodeDescriptor { // 14 bytes
  UInt32    fLink;
  UInt32    bLink;
  SInt8     kind;    // see above
  UInt8     height;
  UInt16    numRecords;
  UInt16    reserved;
};

struct BTHeaderRec {
  SInt16    treeDepth;
  SInt32    rootNode;
  SInt32    leafRecords;
  SInt32    firstLeafNode;
  SInt32    lastLeafNode;
  SInt16    nodeSize;
  SInt16    maxKeyLength;
  SInt32    totalNodes;
  SInt32    freeNodes;
  UInt8     reserved[78];
};

static inline SInt16 DTNodeRecordOffset(BTNodeDescriptor *node, short i) {
  return ((SInt16 *) node)[(NodeSize/2-1) - i];
}

static inline SInt16 DTNodeFreeSpaceOffset(BTNodeDescriptor *node) {
  return DTNodeRecordOffset(node, node->numRecords);
}

static inline SInt16 DTNodeFreeSpace(BTNodeDescriptor *node) {
  return NodeSize - DTNodeFreeSpaceOffset(node) - ((node->numRecords + 1) * 2);
}

static inline UInt8* DTNodeGetRecord(BTNodeDescriptor *node, short i) {
  return ((UInt8 *) node) + DTNodeRecordOffset(node, i);
}

static SInt16 DTNodeRecordSize(BTNodeDescriptor *node, short i) {
  if (i >= node->numRecords) {
    return 0;
  } else {
    return DTNodeRecordOffset (node, i+1) - DTNodeRecordOffset (node, i);
  }
}

enum { // DTDBRec.kind
  kIconRecKind = 1,
  kApplRecKind = 2,
  kCommentRecKind = 3
};

enum { // DTDBRec.keyLen
  kIconRecKeyLen = 11,
  kApplRecKeyLen = 7,
  kCommentRecKeyLen = 5
};

enum { // DTDBRec.iconRec.iconType

  // icon bitmap data:

  k1BitLargeIconAndMask = 1, // 256 bytes:  32x32 1-bit BW icon, followed by mask
  k4BitLargeIcon        = 2, // 512 bytes:  32x32 4-bit color icon
  k8BitLargeIcon        = 3, // 1024 bytes: 32x32 8-bit color icon
  k1BitSmallIconAndMask = 4, // 64 bytes:   16x16 1-bit BW icon, followed by mask
  k4BitSmallIcon        = 5, // 128 bytes:  16x16 4-bit color icon
  k8BitSmallIcon        = 6, // 256 bytes:  16x16 8-bit color icon

  // kBndlOrKind: When fileType is "paul": fileType containing BNDL (often APPL);
  //                            otherwise: human-readable names from "kind" resource

  kBndlOrKind           = 253,

  // kEasyOpen:   When fileType and creator is 'atco': Easy Open ID (2 bytes)

  kEasyOpen             = 254,

  // kOpenRsrc:   When fileType is "paul": fileTypes for creator
  //              When creator  is "atco": creators that can open fileType

  kOpenRsrc             = 255
};

#pragma options align=mac68k

typedef struct {
  UInt8   keyLen; // see above
  UInt8   kind;   // see above
  union {
    struct {
      OSType  creator;
      OSType  fileType;
      UInt8   iconType; // see above
      UInt8   reserved; // always 0?
      UInt32  iconData; // always 0?
      UInt32  dfOffset;
      UInt16  dfLength;
    } iconRec;
    struct {
      OSType  creator;
      SInt16  idx;      // always negative
      UInt32  appCreationDate;
      UInt32  parID;
      Str31   name;
    } applRec;
    struct {
      long    fileID;
      Str255  str;
    } commentRec;
  };
} DTDBRec, *DTDBRecPtr;

typedef Handle DTDBHandle;

class DTDBIterator {
  private:
    DTDBHandle dtdbHndl;
    long  nodeID;
    short currIdx;
  public:
    DTDBIterator (DTDBHandle db);

    void rewind ();

    Boolean getNextRecord (DTDBRecPtr *recPtr, short *recLen);
    Boolean getCurrentRecord (DTDBRecPtr *recPtr, short *recLen);
    long getRecordID ();
    Boolean findRecordID (long id);
};

inline BTNodeDescriptor* DTDBGetNodePtr (DTDBHandle dtdbHndl, long nodeID) {
  return (BTNodeDescriptor *) (*dtdbHndl + (nodeID * NodeSize));
}

inline BTHeaderRec* DTDBGetBTreeHeader(DTDBHandle dtdbHndl) {
  return (BTHeaderRec *) DTNodeGetRecord(DTDBGetNodePtr (dtdbHndl, 0), 0);
}

inline long DTDBGetSize(DTDBHandle dtdbHndl) {
  return DTDBGetBTreeHeader(dtdbHndl)->totalNodes * NodeSize;
}

UInt8 *DTDBGetBitmap(DTDBHandle dtdbHndl, SInt32 &nodeID);
Boolean DTDBIsNodeUsed(DTDBHandle dtdbHndl, SInt32 nodeID);

DTDBHandle LoadVolumeDesktopDB (int vRefNum);