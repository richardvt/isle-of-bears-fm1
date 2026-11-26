// The FM-1 rules engine (app/iob.h, through tests/iob_cli) against the web version's (its src/game.html, through its
// tools/oracle/js-engine.mjs): random games, every step's whole state and legal actions compared.
//   IOB_WEB=../isle-of-bears node tests/oracle.mjs [games=2000] [firstSeed=1]
import { spawn, execFileSync } from 'node:child_process';
import readline from 'node:readline';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const here = path.dirname(fileURLToPath(import.meta.url));
const web = path.resolve(process.env.IOB_WEB || path.join(here, '..', '..', 'isle-of-bears'));
const { createJsEngine } = await import(path.join(web, 'tools/oracle/js-engine.mjs'));
const { SeededRandom } = await import(path.join(web, 'tools/oracle/rng.mjs'));
const bin = path.join(here, 'build', 'iob_cli');
execFileSync('mkdir', ['-p', path.join(here, 'build')]);
execFileSync('cc', ['-O2', '-std=c99', '-Wall', '-Wextra', '-Wno-unused-function', '-o', bin, path.join(here, 'iob_cli.c')], { stdio: 'inherit' });

const games = Number(process.argv[2] || 2000), first = Number(process.argv[3] || 1);
const c = spawn(bin);
const rl = readline.createInterface({ input: c.stdout });
const lines = [];
let waiter = null;
rl.on('line', l => { if (waiter) { const w = waiter; waiter = null; w(l); } else lines.push(l); });
const ask = cmd => new Promise(res => { c.stdin.write(cmd + '\n'); if (lines.length) res(lines.shift()); else waiter = res; });
const canon = o => JSON.stringify(o, (k, v) => v);
// digests: the hands and discards compared as sets (their order is the UI's), the decks in order (they are drawn)
const norm = d => ({ ...d, h: { W: [...d.h.W].sort(), B: [...d.h.B].sort() }, x: { W: [...d.x.W].sort(), B: [...d.x.B].sort() } });

let steps = 0, wins = { W: 0, B: 0, draw: 0, none: 0 };
for (let g = 0; g < games; g++) {
  const seed = first + g, firstTurn = seed % 2 ? 'B' : 'W';
  const js = createJsEngine(seed);
  js.newGame(firstTurn);
  await ask(`new ${seed} ${firstTurn}`);
  const pick = new SeededRandom(seed * 7919 + 13);
  let k = 0;
  for (; k < 600 && !js.winner(); k++) {
    const jd = canon(norm(js.digest())), cd = canon(norm(JSON.parse(await ask('digest'))));
    if (jd !== cd) { console.error(`✗ seed ${seed} step ${k}: state differs\n JS ${jd}\n C  ${cd}`); process.exit(1); }
    const jl = js.flowOpen() ? js.legalActions() : js.hand().map(u => 'card:' + u).sort();
    const cl = (await ask('legal')).split(' ').filter(Boolean).sort();
    if (jl.join(' ') !== cl.join(' ')) { console.error(`✗ seed ${seed} step ${k}: legal differs\n JS ${jl}\n C  ${cl}\n${jd}`); process.exit(1); }
    if (!jl.length) { console.error(`✗ seed ${seed} step ${k}: no legal action, no winner`); process.exit(1); }
    const a = jl[Math.floor(pick.next() * jl.length)];
    if (js.flowOpen()) js.apply(a); else js.playCard(a.slice(5));
    const r = await ask('do ' + a);
    if (r !== 'ok') { console.error(`✗ seed ${seed} step ${k}: C says ${r}`); process.exit(1); }
    steps++;
  }
  const jd = canon(norm(js.digest())), cd = canon(norm(JSON.parse(await ask('digest'))));
  if (jd !== cd) { console.error(`✗ seed ${seed} end: state differs\n JS ${jd}\n C  ${cd}`); process.exit(1); }
  wins[js.winner() || 'none']++;
}
c.stdin.end();
console.log(`✓ ${games} games, ${steps} steps: the FM-1 engine matches the web's (wins ${JSON.stringify(wins)})`);
