import assert from 'node:assert/strict';
import {extractAppImage, parseFirmwareManifest} from '../install-slot/extract-app-image.js';
import {packNameBlob, unpackNameBlob, sanitizeDisplayName} from '../install-slot/name-blob.js';

function fixture(subtype, offset) {
  const b = new Uint8Array(0x50000).fill(0xff), v = new DataView(b.buffer);
  b.set([0xaa,0x50,0,subtype],0x8000);
  v.setUint32(0x8004,offset,true);v.setUint32(0x8008,0x10000,true);
  b.fill(0,offset,offset+112);b[offset]=0xe9;b[offset+1]=1;b[offset+12]=5;
  b.fill(0xab,offset+32,offset+96);
  v.setUint32(offset+24,0x3c010000,true);v.setUint32(offset+28,64,true);
  return b;
}
const factory=fixture(0,0x30000);
assert.equal(extractAppImage(factory).length,112);
const ota=fixture(0x10,0x20000),v=new DataView(ota.buffer);
ota.set([0xaa,0x50,0,0x11],0x8020);v.setUint32(0x8024,0x40000,true);v.setUint32(0x8028,0x10000,true);
assert.equal(extractAppImage(ota).length,112);
assert.equal(parseFirmwareManifest(ota).app.offset,0x20000);
ota[0x40000]=0xe9;
assert.throws(()=>extractAppImage(ota),/Ambiguous/);
const bad=fixture(0,0x20000);new DataView(bad.buffer).setUint32(0x8008,16,true);
assert.throws(()=>extractAppImage(bad),/declared partition/);
assert.equal(unpackNameBlob(packNameBlob('随身小智')),'随身小智');
assert.equal(packNameBlob('\ud800'),null);
assert.equal(new TextEncoder().encode(sanitizeDisplayName('智'.repeat(15))).length,30);
console.log('Zion extract: PASS (factory offset, OTA-only, ambiguity, partition bounds, UTF-8)');
