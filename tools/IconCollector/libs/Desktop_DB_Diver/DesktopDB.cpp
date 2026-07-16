#include <Memory.h>
#include <stdexcept>

#include "DesktopDB.h"

DTDBHandle LoadVolumeDesktopDB (int vRefNum) {
  FSSpec spec;
  short refNum;
  long fileSize;

  OSErr err = FSMakeFSSpec (vRefNum, 0, "\p:Desktop DB", &spec);
  if (err) throw err;

  err = FSpOpenDF (&spec, fsRdPerm, &refNum);
  if (err) throw err;

  err = GetEOF (refNum, &fileSize);
  if (err) throw err;

  Handle dtdbHndl = NewHandle(fileSize);
  if(!dtdbHndl) throw MemError();

  HLock(dtdbHndl);
  err = FSRead(refNum, &fileSize, *dtdbHndl);
  if (err) throw err;
  //HUnlock(dtdbHndl);

  err = FSClose(refNum);
  if (err) throw err;

  return dtdbHndl;
}

UInt8 *DTDBGetBitmap(DTDBHandle dtdbHndl, SInt32 &nodeID) {
  enum {
    kNumNodesInMapNode = 3936L
  };

  UInt8 *bitmap;

  if (nodeID < 2048) {
    bitmap = DTNodeGetRecord(DTDBGetNodePtr (dtdbHndl, 0), 2);
  }
  else if (nodeID < 2048 + kNumNodesInMapNode) {
    nodeID -= 2048;

    SInt32  mapNodeNr = (nodeID / kNumNodesInMapNode);
    SInt32  mapNodeID = 0;

    for (short i = 0; i <= mapNodeNr; i++) {
      mapNodeID = DTDBGetNodePtr(dtdbHndl, mapNodeID)->fLink;
    }

    if (DTDBGetNodePtr(dtdbHndl, mapNodeID)->kind != kBTMapNode) {
      throw (kBTreeInvalidErr);
    }

    bitmap = DTNodeGetRecord(DTDBGetNodePtr(dtdbHndl, mapNodeID), 0);
    nodeID -= kNumNodesInMapNode * mapNodeNr;
  }
  return bitmap;
}

Boolean DTDBIsNodeUsed(DTDBHandle dtdbHndl, SInt32 nodeID) {
  UInt8 *bitmap = DTDBGetBitmap(dtdbHndl, nodeID);

  SInt32  bytePos = nodeID / 8;
  SInt32  bitPos = nodeID % 8;

  return (bitmap[bytePos] & (0x80 >> bitPos)) != 0;
}

DTDBIterator::DTDBIterator (DTDBHandle db) : dtdbHndl(db) {
  rewind();
}

Boolean DTDBIterator::getNextRecord (DTDBRecPtr *recPtr, short *recLen) {
  short nextIdx = currIdx + 1;

  BTNodeDescriptor  *node = DTDBGetNodePtr (dtdbHndl, nodeID);

  if (nextIdx >= node->numRecords) {
    // Once we've read all the records in the
    // current node, advance to the next node
    if (node->fLink == 0) {
      return false;
    }
    nodeID = node->fLink;
    nextIdx = 0;
    node = DTDBGetNodePtr (dtdbHndl, nodeID);
  }

  currIdx = nextIdx;
  *recPtr = (DTDBRecPtr) DTNodeGetRecord (node, currIdx);
  *recLen = DTNodeRecordSize (node, currIdx);
  return true;
}

Boolean DTDBIterator::getCurrentRecord (DTDBRecPtr *recPtr, short *recLen) {
  BTNodeDescriptor *node = DTDBGetNodePtr (dtdbHndl, nodeID);
  *recPtr = (DTDBRecPtr) DTNodeGetRecord (node, currIdx);
  *recLen = DTNodeRecordSize (node, currIdx);
  return true;
}

void DTDBIterator::rewind () {
  nodeID = DTDBGetBTreeHeader(dtdbHndl)->firstLeafNode;
  currIdx = -1;
}

long DTDBIterator::getRecordID () {
  return nodeID * 256 + currIdx;
}

Boolean DTDBIterator::findRecordID (long id) {
  nodeID = id / 256;
  currIdx = id & 255;
}