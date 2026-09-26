#!/usr/bin/env node
// Exports UE-Extended's GameSettings table (src/games/ue-extended/addon.cpp, GAME_SETTINGS) as JSON,
// so a host app can tell which Unreal games the add-on has tuned defaults for without reading C++.
// UE-Extended is marat569's add-on (marat569/renodx, src/games/ue-extended); this fork only adds the
// RenoDX host API. The table is theirs; this script just reads it.
//
//   node scripts/ue-extended-games.mjs [--out path] [--commit sha] [--ref name]
//
// Output (a superset of what OptiDLSS5-UI's tools/gen-ue-extended-table.js writes, same top-level keys):
//   {
//     "source": { repo, file, ref, commit, author, hostApiVersion },
//     "generated": "YYYY-MM-DD",
//     "count": N,
//     "match": "...how the add-on picks an entry...",
//     "games": {
//       "<key>": {
//         "nativeHdr": bool,          // defaults set Set_Path 0: the game's own HDR (Engine.ini cvars needed)
//         "matchBy": "exe"|"productName",
//         "defaults": { "<setting key>": number, ... },
//         "additionalSettings": ["<setting key or label>", ...],
//         "customShaders": ["0x........", ...]
//       }
//     }
//   }
//
// It fails (exit 1) when the number of entries it parsed differs from the number of `GameSettings{`
// openings in the table, so a table layout it does not understand breaks the build instead of
// shipping a short list.

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const FILE = 'src/games/ue-extended/addon.cpp';

function arg(name, fallback = null) {
  const i = process.argv.indexOf(name);
  return i > 0 && process.argv[i + 1] ? process.argv[i + 1] : fallback;
}

// Removes comments, keeping string literals and line structure intact.
function stripComments(src) {
  let out = '';
  for (let i = 0; i < src.length; i++) {
    const c = src[i];
    if (c === '"') {
      let j = i + 1;
      while (j < src.length && src[j] !== '"') j += src[j] === '\\' ? 2 : 1;
      out += src.slice(i, j + 1);
      i = j;
    } else if (c === '/' && src[i + 1] === '/') {
      while (i < src.length && src[i] !== '\n') i++;
      out += '\n';
    } else if (c === '/' && src[i + 1] === '*') {
      i = src.indexOf('*/', i + 2) + 1;
    } else {
      out += c;
    }
  }
  return out;
}

// Top-level {...} groups of `text` (brace-balanced, strings skipped), as inner strings.
function groups(text) {
  const result = [];
  let depth = 0;
  let start = -1;
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (c === '"') {
      i++;
      while (i < text.length && text[i] !== '"') i += text[i] === '\\' ? 2 : 1;
    } else if (c === '{') {
      if (depth++ === 0) start = i + 1;
    } else if (c === '}') {
      if (--depth === 0) result.push(text.slice(start, i));
    }
  }
  return result;
}

const unquote = (s) => s.replace(/\\(.)/g, '$1');

function parse(cpp) {
  const code = stripComments(cpp);
  const constants = {};
  for (const m of code.matchAll(/const\s+auto\s+(UPGRADE_TYPE_\w+)\s*=\s*([\d.]+)f?\s*;/g)) {
    constants[m[1]] = Number(m[2]);
  }
  const at = code.indexOf('GAME_SETTINGS = {');
  if (at < 0) throw new Error(`${FILE} has no GAME_SETTINGS table`);
  const table = groups(code.slice(at + 'GAME_SETTINGS = '.length))[0];
  const openings = (table.match(/GameSettings\s*\{/g) || []).length;

  const games = {};
  for (const entry of groups(table)) {
    const key = /^\s*"((?:[^"\\]|\\.)*)"\s*,\s*GameSettings\s*\{/.exec(entry);
    if (!key) throw new Error(`unexpected GAME_SETTINGS entry: ${entry.slice(0, 80)}`);
    const name = unquote(key[1]);
    const body = groups(entry)[0];
    // Two spellings. `GameSettings{ {"Key", v}, ... }` is the initializer-list constructor taking the
    // defaults alone (list-initialisation prefers it); `GameSettings{ { {"Key", v}, ... }, {settings},
    // {hashes} }` passes all three arguments. Tell them apart by whether the first group is a pair.
    const parts = groups(body);
    const [defaultsText = '', additionalText = '', shadersText = ''] =
      parts.length === 0 || /^\s*"/.test(parts[0]) ? [body] : parts;

    const defaults = {};
    for (const pair of groups(defaultsText)) {
      const m = /^\s*"((?:[^"\\]|\\.)*)"\s*,\s*([\w.+-]+?)f?\s*$/.exec(pair);
      if (!m) throw new Error(`${name}: cannot read default ${pair.trim()}`);
      const value = m[2] in constants ? constants[m[2]] : Number(m[2]);
      if (!Number.isFinite(value)) throw new Error(`${name}: default ${m[1]} is not a number (${m[2]})`);
      defaults[unquote(m[1])] = value;
    }
    const additionalSettings = groups(additionalText).map((s) => {
      const k = /\.key\s*=\s*"((?:[^"\\]|\\.)*)"/.exec(s) || /\.label\s*=\s*"((?:[^"\\]|\\.)*)"/.exec(s);
      return k ? unquote(k[1]) : '(unnamed)';
    });
    const customShaders = [...shadersText.matchAll(/0x[0-9A-Fa-f]+/g)].map((m) => `0x${m[0].slice(2).toUpperCase()}`);

    if (Object.prototype.hasOwnProperty.call(games, name)) throw new Error(`duplicate key ${name}`);
    games[name] = {
      nativeHdr: defaults.Set_Path === 0,
      matchBy: /\.exe$/i.test(name) ? 'exe' : 'productName',
      defaults,
      additionalSettings,
      customShaders,
    };
  }
  if (Object.keys(games).length !== openings) {
    throw new Error(`parsed ${Object.keys(games).length} entries but the table opens ${openings} GameSettings`);
  }
  return games;
}

const cpp = fs.readFileSync(path.join(ROOT, FILE), 'utf8');
const settingsHpp = fs.readFileSync(path.join(ROOT, 'src/utils/settings.hpp'), 'utf8');
const hostApi = /HOST_API_VERSION\s*=\s*(\d+)u?\s*;/.exec(settingsHpp);
const games = parse(cpp);
const sorted = Object.fromEntries(Object.keys(games).sort((a, b) => a.localeCompare(b, 'en')).map((k) => [k, games[k]]));

const out = {
  source: {
    repo: process.env.GITHUB_REPOSITORY || 'mrcgibb9876-hash/renodx',
    file: FILE,
    ref: arg('--ref', process.env.GITHUB_REF_NAME || null),
    commit: arg('--commit', process.env.GITHUB_SHA || null),
    author: 'marat569 (marat569/renodx, src/games/ue-extended)',
    hostApiVersion: hostApi ? Number(hostApi[1]) : null,
  },
  generated: new Date().toISOString().slice(0, 10),
  count: Object.keys(sorted).length,
  match:
    'The running exe file name first, then the exe ProductName (version resource); both exact and case-sensitive (FindGameSettings).',
  games: sorted,
};

const text = `${JSON.stringify(out, null, 2)}\n`;
const dest = arg('--out');
if (dest) {
  fs.writeFileSync(dest, text);
  console.log(`${out.count} UE-Extended titles -> ${dest} (${Object.values(sorted).filter((g) => g.nativeHdr).length} native HDR)`);
} else {
  process.stdout.write(text);
}
