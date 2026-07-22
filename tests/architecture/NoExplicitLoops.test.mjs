import assert from 'node:assert/strict';
import { readdirSync, readFileSync } from 'node:fs';
import { join, resolve } from 'node:path';
import test from 'node:test';

const collectModules = directory => readdirSync(directory, {
  withFileTypes: true,
}).flatMap((entry) => {
  const path = join(directory, entry.name);
  if (entry.isDirectory()) return collectModules(path);
  return entry.name.endsWith('.mjs') ? [path] : [];
});

test('project modules contain no explicit loops', () => {
  const roots = [
    resolve(import.meta.dirname, '../../js'),
    resolve(import.meta.dirname, '..'),
  ];
  const explicitLoop = /\b(?:for|while)\s*\(|\bdo\s*\{/u;
  const violations = roots
    .flatMap(collectModules)
    .filter(path => explicitLoop.test(readFileSync(path, 'utf8')));

  assert.deepEqual(violations, []);
});
