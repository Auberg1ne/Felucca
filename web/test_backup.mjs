// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// fm1backup.js against a simulated device: 7-bit packing, CRC, manifest checks, a full capture,
// validation before the first write, restore order (live music last) and abort on a failed chunk.
import { BACKUP_IDS, BACKUP_CMD, bkU32, bkR32, bkPack, bkUnpack, bkCrc, bkManifest, readBackup, captureBackup, restoreBackup } from "./fm1backup.js";

let fails = 0;
const ok = (c, what) => { console.log(`${what.padEnd(72)} ${c ? "ok" : "FAIL"}`); if (!c) fails++; };
const throws = (f) => { try { f(); return false; } catch { return true; } };
const athrows = async (f) => { try { await f(); return false; } catch { return true; } };

const rnd = (n, seed) => Uint8Array.from({ length: n }, (_, i) => (i * 131 + seed * 17 + (i >> 3)) & 255);
ok(bkR32(bkU32(0xdeadbeef)) === 0xdeadbeef && bkCrc(new TextEncoder().encode("123456789")) === 0xcbf43926, "backup: numbers and CRC-32");
const b = rnd(1000, 1);
ok(bkUnpack(bkPack(b), b.length).every((v, i) => v === b[i]), "backup: 7-bit pack / unpack round trip");
ok(throws(() => bkUnpack([...bkPack(b.subarray(0, 14)), 0], 14)), "backup: trailing bytes refused");

/* a device: objects by id, sample slots, the order of writes */
function device(objs, opt = {}) {
  const ids = opt.ids || BACKUP_IDS;
  const d = { calls: [], samples: new Map(), objs: new Map(objs), log: [], staged: null };
  d.request = async ([cmd, a], options) => {
    d.calls.push({ cmd, a: [...a], options });
    if (cmd === 15) {
      if (opt.infoError) throw new Error("disconnected");
      return opt.info || [opt.sampleSlots || 8, 80, ...Array(opt.sampleSlots || 8).fill([0, 0, 0]).flat()];
    }
    if (cmd === BACKUP_CMD.LIST) {
      const out = [1, 0, ids.length];
      for (const id of ids) { const v = d.objs.get(id) || new Uint8Array(0); out.push(id, ...bkU32(v.length), ...bkU32(v.length ? bkCrc(v) : 0)); }
      return out;
    }
    if (cmd === BACKUP_CMD.GET) {
      const id = a[0], off = bkR32(a, 1), n = a[6] | a[7] << 7, v = d.objs.get(id);
      if (opt.changeOnGet === id && off === 0) v[0] ^= 1;
      return [id, 0, ...bkU32(off), n & 127, n >> 7, ...bkPack(v.subarray(off, off + n))];
    }
    if (cmd === BACKUP_CMD.PUT) {
      const [op, id] = a;
      if (op === 0 && id > (opt.maxId ?? 9)) { d.log.push(`refused ${id}`); return [op, id, 1]; }   // (an older firmware)
      if (op === 0) { d.staged = { id, size: bkR32(a, 2), crc: bkR32(a, 7), bytes: [] }; return [op, id, 0]; }
      if (op === 1) {
        if (opt.failChunk === id) return [op, id, 2];
        const off = bkR32(a, 2), p = a.slice(7), n = Math.min(256, d.staged.size - off);
        d.staged.bytes.push(...bkUnpack(p, n)); return [op, id, 0];
      }
      if (op === 2) { const s = Uint8Array.from(d.staged.bytes); const rc = bkCrc(s) === d.staged.crc ? 0 : 2; if (!rc) { d.objs.set(id, s); d.log.push(id); } return [op, id, rc]; }
      if (op === 3) { d.log.push(`abort ${id}`); return [op, id, 0]; }
    }
    if ([11, 12, 13, 14].includes(cmd)) {
      if (opt.sampleFail === cmd) throw new Error("lost sample acknowledgement");
      const slot = a[0];
      if (slot >= (opt.sampleSlots || 8)) throw new Error("invalid slot");
      if (cmd === 11) {
        if (a.length !== 4) throw new Error("bad BEGIN framing");
        const len = a[1] | a[2] << 7 | a[3] << 14;
        d.sampleStage = { slot, len, next: 512, bytes: new Uint8Array(512 + len) };
      } else if (cmd === 12) {
        const u = d.sampleStage, off = a[1] | a[2] << 7 | a[3] << 14;
        const count = Math.min(256, u.bytes.length - off), bytes = bkUnpack(a.slice(4), count);
        if (u.slot !== slot || off !== u.next || !count || off + count > u.bytes.length) throw new Error("bad WRITE framing");
        u.bytes.set(bytes, off); u.next += count;
        return [slot, a[1], a[2], a[3], 0];
      } else if (cmd === 13) {
        const u = d.sampleStage, h = bkUnpack(a.slice(1), 480);
        if (u.slot !== slot || u.next !== u.bytes.length || new DataView(h.buffer).getUint32(16, true) !== u.len) throw new Error("bad END framing");
        u.bytes.set(h); d.samples.set(slot, u.bytes); d.sampleStage = null; d.log.push(32 + slot);
      } else { if (a.length !== 1) throw new Error("bad ERASE framing"); d.samples.delete(slot); d.log.push(32 + slot); }
      if (opt.lateAck === cmd) throw new Error("committed but acknowledgement lost");
      return [slot, 0];
    }
    throw new Error(`unexpected ${cmd}`);
  };
  return d;
}
const objs = [[0, rnd(3388, 2)], [1, rnd(1200, 3)], [2, rnd(3388, 4)], [6, rnd(3080, 5)]];
const dev = device(objs);
const file = await captureBackup(dev.request, "TEST");
ok(file.objects.length === BACKUP_IDS.length && file.objects[0].size === 3388 && file.objects[3].size === 0, "backup: capture lists every object, empty ones as 0");
ok(readBackup(JSON.stringify(file)).objects[2].bytes.every((v, i) => v === objs[2][1][i]), "backup: capture -> file -> bytes round trip");
ok(await athrows(() => captureBackup(device(objs.map(([i, v]) => [i, v.slice()]), { changeOnGet: 2 }).request, "TEST")), "backup: a device that changes during capture fails the capture");

const bad = JSON.parse(JSON.stringify(file)); bad.objects[2].crc ^= 1;
ok(throws(() => readBackup(bad)), "backup: a damaged file is refused");
const noRun = JSON.parse(JSON.stringify(file)); noRun.objects[0] = { ...noRun.objects[0], size: 0, crc: 0, data: "" };
ok(throws(() => readBackup(noRun)), "backup: a file without the current music is refused");
ok(throws(() => bkManifest([1, 0, 3])), "backup: a short manifest is refused");

const target = device([]);
const damaged = JSON.parse(JSON.stringify(file)); damaged.objects[6].crc ^= 1;
ok(await athrows(() => restoreBackup(target.request, damaged)) && target.log.length === 0, "backup: restore validates every byte before the first write");
await restoreBackup(target.request, file);
const order = target.log.filter((x) => typeof x === "number");
ok(order.at(-1) === 0 && order.at(-2) === 1 && order.indexOf(2) < order.indexOf(1), "backup: restore order: projects, banks, samples, settings, live music last");
ok([0, 1, 2, 6].every((id) => target.objs.get(id).every((v, i) => v === objs.find((o) => o[0] === id)[1][i])), "backup: restored objects equal the source");
{   /* the FM6 patch bank (id 8) and the archives of firmware before it (11 objects, no id 8) */
  ok(BACKUP_IDS.length === 18 && file.objects.some((o) => o.id === 8) && file.objects.some((o) => o.id === 9),
    "backup: 18 objects: id 8 (the retired FM6 bank, empty) and id 9 (the user presets' FM6 patches)");
  const v2 = JSON.parse(JSON.stringify(file)); v2.objects = v2.objects.filter((o) => o.id !== 9 && o.id < 35);
  ok(readBackup(v2).objects.length === 12, "backup: an archive of 1.0..1.0.2 (12 objects, the bank as id 8) still reads");
  const old = JSON.parse(JSON.stringify(file)); old.objects = old.objects.filter((o) => o.id !== 8 && o.id !== 9 && o.id < 35);
  ok(readBackup(old).objects.length === 11, "backup: an archive of the 11 objects before FM6 still reads");
  const odd = JSON.parse(JSON.stringify(file)); odd.objects = odd.objects.filter((o) => o.id !== 33);
  ok(throws(() => readBackup(odd)), "backup: an archive missing another object is refused");
}
{   /* 1.0.3: an old archive with a bank restores its id 8 (after 6, 7); a new archive onto 1.0.2 skips id 9 */
  const bankBytes = rnd(3472, 8), patches = rnd(3728, 9);
  const v2dev = device([...objs, [8, bankBytes]], { ids: BACKUP_IDS.filter((id) => id < 35), sampleSlots: 3 });
  const v2file = await captureBackup(async ([cmd, a]) => {   // (a 1.0.2 device: 12 objects)
    const r = await v2dev.request([cmd, a]);
    if (cmd !== BACKUP_CMD.LIST) return r;
    const n = r[2], keep = [];
    for (let i = 0; i < n; i++) if (r[3 + i * 11] !== 9) keep.push(...r.slice(3 + i * 11, 14 + i * 11));
    return [1, 0, keep.length / 11, ...keep];
  }, "1.0.2");
  const to103 = device([]);
  await restoreBackup(to103.request, v2file);
  const lg = to103.log.filter((x) => typeof x === "number");
  ok(v2file.objects.length === 12 && lg.indexOf(8) > lg.indexOf(7) && lg.indexOf(7) > lg.indexOf(6) &&
     to103.objs.get(8).every((v, i) => v === bankBytes[i]), "backup: a 1.0.2 archive (id 8, the bank) restores, its bank after the user presets");
  const newFile = await captureBackup(device([...objs, [9, patches]], { ids: BACKUP_IDS.filter((id) => id < 35), sampleSlots: 3 }).request, "1.0.3");
  const to102 = device([], { maxId: 8, sampleSlots: 3 });
  await restoreBackup(to102.request, newFile);
  ok(to102.log.includes("refused 9") && !to102.objs.has(9) && to102.log.at(-1) === 0,
    "backup: a 1.0.3 archive onto older firmware: id 9 skipped, the rest restored");
  const to103b = device([]);
  await restoreBackup(to103b.request, newFile);
  ok(to103b.objs.get(9).every((v, i) => v === patches[i]), "backup: a 1.0.3 archive restores id 9");
}
const failing = device([], { failChunk: 2 });
ok(await athrows(() => restoreBackup(failing.request, file)) && failing.log.includes("abort 2") && !failing.log.includes(0), "backup: a refused chunk aborts that object and stops before the live music");


/* Eight-slot archives use explicit independent fixtures, not just BACKUP_IDS. */
const profiles = [
  [0,1,2,3,4,5,6,7,32,33,34], [0,1,2,3,4,5,6,7,8,32,33,34],
  [0,1,2,3,4,5,6,7,8,9,32,33,34], [0,1,2,3,4,5,6,7,8,9,32,33,34,35,36,37,38,39],
];
for (const ids of profiles) {
  const f = await captureBackup(device(objs, { ids }).request, "profile");
  const target = device([]);
  if(ids.length < 18) for(let k=3;k<8;k++) target.samples.set(k,Uint8Array.of(k));
  await restoreBackup(target.request, f);
  ok(readBackup(f).objects.map((o) => o.id).join() === ids.join(), `S3 exact ${ids.length}-object manifest and file profile`);
  ok(target.calls[0].cmd === 15 && target.log.every((id) => typeof id !== "number" || id < 35 || ids.length === 18),
    `S3 ${ids.length}-object restore preflights first and leaves absent high slots alone`);
  if(ids.length < 18) {
    ok([3,4,5,6,7].every(k=>target.samples.get(k)?.[0]===k),`S3 ${ids.length}-object legacy restore preserves populated USR4–8`);
    const three=device([],{sampleSlots:3});await restoreBackup(three.request,f);
    ok(three.calls[0].cmd===15,`S3 ${ids.length}-object legacy restore works on three-slot destination`);
  }
}
for (const mode of ["duplicate", "unsupported", "missing", "reordered"]) {
  const f = JSON.parse(JSON.stringify(file));
  if (mode === "duplicate") f.objects[17].id = 38;
  if (mode === "unsupported") f.objects[17].id = 40;
  if (mode === "missing") f.objects.pop();
  if (mode === "reordered") [f.objects[15], f.objects[16]] = [f.objects[16], f.objects[15]];
  const d = device([]);
  ok(await athrows(() => restoreBackup(d.request, f)) && !d.calls.length, `S3 ${mode} archive rejected before all requests`);
}
const small = device([], { sampleSlots: 3 });
ok(await athrows(() => restoreBackup(small.request, file)) && small.calls.length === 1 && !small.log.length,
  "S3 eight-slot archive rejected on three-slot destination before any mutation, including empty high slots");
for (const info of [Object.assign(Array(26),{0:8,1:80}), [8,80], [9,80], [8,0], [8,80,...Array(8).fill([0,0,0]).flat(),0]]) {
  const d = device([], { info });
  ok(await athrows(() => restoreBackup(d.request, file)) && !d.log.length && d.calls.length === 1,
    "S3 malformed destination INFO fails closed before any mutation");
}
const disconnected = device([], { infoError: true });
ok(await athrows(() => restoreBackup(disconnected.request, file)) && !disconnected.log.length,
  "S3 unavailable capability read never falls back to assumed slots");
function sample(k) {
  const bytes = new Uint8Array(512 + 513), v = new DataView(bytes.buffer);
  bytes.set(rnd(513, k), 512); v.setUint32(0, 0x504d5346, true); v.setUint16(4, 1, true);
  bytes[6] = 1; bytes[8] = 65 + k; v.setUint32(16, 513, true); v.setUint32(20, bkCrc(bytes.subarray(512)), true);
  v.setUint32(36, 1026, true); v.setUint32(44, 1025, true); v.setUint32(48, 65536, true);
  v.setInt16(54, 960, true); bytes[58] = 127;
  return bytes;
}
const populated = await captureBackup(device([...objs, ...Array.from({ length: 8 }, (_, k) => [32 + k, sample(k)])]).request, "S3");
const restored = device([]); await restoreBackup(restored.request, populated);
for (let k = 0; k < 8; k++) {
  const calls = restored.calls.filter((c) => c.cmd >= 11 && c.cmd <= 14 && c.a[0] === k), bytes = sample(k);
  ok(JSON.stringify(calls[0].a) === JSON.stringify([k,1,4,0]) && calls[0].cmd === 11 && calls.at(-1).cmd === 13 &&
    calls.filter((c) => c.cmd === 12).map((c) => c.a[1] | c.a[2] << 7 | c.a[3] << 14).join() === "512,768,1024" &&
    bkUnpack(calls.at(-1).a.slice(1),480).every((b,i) => b === bytes[i]) &&
    restored.samples.get(k).every((b,i) => b === bytes[i]) && calls.every((c) => c.options.retries === 0),
    `S3 USR${k+1} exact BEGIN length, sequential WRITE, END header and sample byte round trip`);
}
for (const cmd of [11,12,13]) {
  const d = device([], { sampleFail: cmd });
  ok(await athrows(() => restoreBackup(d.request, populated)) && d.calls.filter((c) => c.cmd === cmd).length === 1 &&
    !d.log.includes(0) && !d.samples.size, `S3 lost acknowledgement at ${cmd} stops restore without destructive retries`);
}

for (const mode of ["slot","offset","short","rc"]) {
  const d=device([]), request=async(r,o)=>{
    const a=await d.request(r,o);
    if(r[0]===12){if(mode==="slot")a[0]=7;if(mode==="offset")a[1]=1;if(mode==="short")return [0,0];if(mode==="rc")a[4]=3;}
    return a;
  };
  ok(await athrows(()=>restoreBackup(request,populated)) && !d.calls.some(c=>c.cmd===13),`S3 restore wrong ${mode} WRITE reply stops before END`);
}
const quarantined=device([]);
ok(await athrows(()=>restoreBackup(async(r,o)=>r[0]===11?[r[1][0],1]:quarantined.request(r,o),populated)) &&
  !quarantined.calls.some(c=>c.cmd===12 || c.cmd===13),"S3 unavailable/quarantined sample failure is not bypassed");
const metadata=JSON.parse(JSON.stringify(populated));
const changed=sample(0);new DataView(changed.buffer).setInt16(54,-1,true);
metadata.objects[10]={...metadata.objects[10],crc:bkCrc(changed),data:Buffer.from(changed).toString("base64")};
const reject=device([]);
ok(await athrows(()=>restoreBackup(reject.request,metadata)) && !reject.calls.length,"S3 invalid sample metadata rejected before any restore mutation");

for(const cmd of [11,13]) {
  const d=device([],{lateAck:cmd});
  ok(await athrows(()=>restoreBackup(d.request,populated)) && d.calls.filter(c=>c.cmd===cmd).length===1 &&
    (cmd===11?!!d.sampleStage:d.samples.has(0)) && !d.log.includes(0),
    `S3 applied ${cmd} with lost acknowledgement is unresolved, never automatically retried`);
}
console.log(fails ? `BACKUP WEB TESTS FAILED (${fails})` : "backup web tests passed");
process.exit(fails ? 1 : 0);
