'use strict';

const fs = require('fs');
const path = require('path');

const repo = path.resolve(__dirname, '..');
const mappingDirectory = path.join(repo, 'third_party', 'mcp', '1.8.9');
const srgPath = path.join(mappingDirectory, 'joined.srg');
const fieldsCsvPath = path.join(mappingDirectory, 'fields.csv');
const methodsCsvPath = path.join(mappingDirectory, 'methods.csv');
const outputPath = path.join(repo, 'TagEssentialsMod', 'LunarMappings.generated.h');

function readMcpNames(csvPath) {
  const result = new Map();
  const lines = fs.readFileSync(csvPath, 'utf8').replace(/^\uFEFF/, '').split(/\r?\n/);
  for (let index = 1; index < lines.length; index += 1) {
    const match = /^([^,]+),([^,]+),/.exec(lines[index]);
    if (match) result.set(match[1], match[2]);
  }
  return result;
}

function quote(value) {
  return JSON.stringify(value).replace(/\\u([0-9a-fA-F]{4})/g, (_, hex) => {
    return `\\x${hex.slice(0, 2)}\\x${hex.slice(2)}`;
  });
}

const fieldNames = readMcpNames(fieldsCsvPath);
const methodNames = readMcpNames(methodsCsvPath);
const classes = [];
const fields = [];
const methods = [];

for (const line of fs.readFileSync(srgPath, 'utf8').split(/\r?\n/)) {
  let match = /^CL: (\S+) (\S+)$/.exec(line);
  if (match) {
    classes.push({ obfuscated: match[1], named: match[2] });
    continue;
  }

  match = /^FD: (\S+) (\S+)$/.exec(line);
  if (match) {
    const leftSlash = match[1].lastIndexOf('/');
    const rightSlash = match[2].lastIndexOf('/');
    if (leftSlash > 0 && rightSlash > 0) {
      const srgName = match[2].slice(rightSlash + 1);
      fields.push({
        owner: match[2].slice(0, rightSlash),
        obfuscated: match[1].slice(leftSlash + 1),
        named: fieldNames.get(srgName) || srgName,
      });
    }
    continue;
  }

  match = /^MD: (\S+) (\S+) (\S+) (\S+)$/.exec(line);
  if (match) {
    const leftSlash = match[1].lastIndexOf('/');
    const rightSlash = match[3].lastIndexOf('/');
    if (leftSlash > 0 && rightSlash > 0) {
      const srgName = match[3].slice(rightSlash + 1);
      methods.push({
        owner: match[3].slice(0, rightSlash),
        obfuscated: match[1].slice(leftSlash + 1),
        descriptor: match[2],
        named: methodNames.get(srgName) || srgName,
      });
    }
  }
}

classes.sort((a, b) => a.obfuscated.localeCompare(b.obfuscated));
fields.sort((a, b) =>
  a.owner.localeCompare(b.owner) || a.obfuscated.localeCompare(b.obfuscated));
methods.sort((a, b) =>
  a.owner.localeCompare(b.owner) ||
  a.obfuscated.localeCompare(b.obfuscated) ||
  a.descriptor.localeCompare(b.descriptor));

const output = [];
output.push('#pragma once');
output.push('');
output.push('// Generated from MCP 1.8.9 joined.srg plus stable_22 CSV names.');
output.push('// This is an altered mapping-data derivative; see third_party/mcp/LICENSE.txt.');
output.push('// Regenerate with: node tools\\generate_lunar_mappings.js');
output.push('struct LunarClassNameMapping { const char* obfuscated; const char* named; };');
output.push('struct LunarFieldNameMapping { const char* owner; const char* obfuscated; const char* named; };');
output.push('struct LunarMethodNameMapping { const char* owner; const char* obfuscated; const char* descriptor; const char* named; };');
output.push('');
output.push('static const LunarClassNameMapping kLunarClassNameMappings[] = {');
for (const entry of classes) {
  output.push(`    { ${quote(entry.obfuscated)}, ${quote(entry.named)} },`);
}
output.push('};');
output.push('');
output.push('static const LunarFieldNameMapping kLunarFieldNameMappings[] = {');
for (const entry of fields) {
  output.push(`    { ${quote(entry.owner)}, ${quote(entry.obfuscated)}, ${quote(entry.named)} },`);
}
output.push('};');
output.push('');
output.push('static const LunarMethodNameMapping kLunarMethodNameMappings[] = {');
for (const entry of methods) {
  output.push(`    { ${quote(entry.owner)}, ${quote(entry.obfuscated)}, ${quote(entry.descriptor)}, ${quote(entry.named)} },`);
}
output.push('};');

fs.writeFileSync(outputPath, `${output.join('\n')}\n`, 'utf8');
process.stdout.write(
  `Generated ${path.relative(repo, outputPath)}: ` +
  `${classes.length} classes, ${fields.length} fields, ${methods.length} methods\n`);
