'use strict';

const fs = require('fs');
const path = require('path');

const sourcePath = path.resolve(__dirname, '..', 'TagEssentialsMod', 'TagEssentialsMod.cpp');
let source = fs.readFileSync(sourcePath, 'utf8');

// The compatibility wrappers must call JNI directly. Protect their bodies so
// this mechanical rewrite remains safe and idempotent when it is run again.
const protectedBlocks = [];
source = source.replace(
  /(?:jfieldID|jmethodID) Get(?:Static)?(?:Field|Method)IDCompat\([\s\S]*?^\}/gm,
  (block) => {
    const marker = `__TAGESSENTIALS_JNI_COMPAT_BLOCK_${protectedBlocks.length}__`;
    protectedBlocks.push(block);
    return marker;
  },
);

const replacements = [
  ['g_env->GetStaticMethodID(', 'GetStaticMethodIDCompat(g_env, '],
  ['g_env->GetStaticFieldID(', 'GetStaticFieldIDCompat(g_env, '],
  ['g_env->GetMethodID(', 'GetMethodIDCompat(g_env, '],
  ['g_env->GetFieldID(', 'GetFieldIDCompat(g_env, '],
  ['env->GetStaticMethodID(', 'GetStaticMethodIDCompat(env, '],
  ['env->GetStaticFieldID(', 'GetStaticFieldIDCompat(env, '],
  ['env->GetMethodID(', 'GetMethodIDCompat(env, '],
  ['env->GetFieldID(', 'GetFieldIDCompat(env, '],
];

let total = 0;
for (const [before, after] of replacements) {
  const count = source.split(before).length - 1;
  source = source.split(before).join(after);
  total += count;
  process.stdout.write(`${before} ${count}\n`);
}

for (let index = 0; index < protectedBlocks.length; index += 1) {
  source = source.replace(`__TAGESSENTIALS_JNI_COMPAT_BLOCK_${index}__`, protectedBlocks[index]);
}

fs.writeFileSync(sourcePath, source, 'utf8');
process.stdout.write(`Rewrote ${total} JNI member lookups in ${sourcePath}\n`);
