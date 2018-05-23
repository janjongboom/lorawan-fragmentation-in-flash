const fs = require('fs');
const Path = require('path');
const execSync = require('child_process').execSync;
const crc64 = require('./calculate-crc64/crc');

const binaryPath = Path.resolve(process.argv[2]);
const tempFilePath = Path.join(__dirname, 'temp.bin');

let manifest = execSync(`manifest-tool create -p "${binaryPath}"`);

// so the format is going to be...
// * 4 bytes size of manifest
// * 4 bytes diff header
// * manifest
// * firmware

let manifestSizeBuffer = Buffer.from([
    manifest.length & 0xff,
    manifest.length >> 8 & 0xff,
    manifest.length >> 16 & 0xff,
    0,
]);

console.log('manifest', manifest);

fs.writeFileSync(Path.join(__dirname, 'temp.manifest'), manifest);

// diff info contains (bool is_diff, 3 bytes for the size of the *old* firmware)
let isDiffBuffer = Buffer.from([ 0, 0, 0, 0 ]);

// now make a temp file which contains signature + class IDs + if it's a diff or not + bin
fs.writeFileSync(tempFilePath, Buffer.concat([ manifestSizeBuffer, isDiffBuffer, manifest, fs.readFileSync(binaryPath) ]));

// Invoke encode_file.py to make packets...
const infile = execSync('python ' + Path.join(__dirname, 'encode_file.py') + ' ' + tempFilePath + ' 204 20').toString('utf-8').split('\n');

// calculate CRC64 hash
const hash = crc64(fs.readFileSync(tempFilePath));
console.log('CRC64 hash is', hash);

const outfile = process.argv[3];

let header;
let fragments = [];

for (let line of infile) {
    if (line.indexOf('Fragmentation header likely') === 0) {
        header = line.replace('Fragmentation header likely: ', '').match(/\b0x(\w\w)\b/g).map(n => parseInt(n));
    }

    else if (line.indexOf('[8, ') === 0) {
        fragments.push(line.replace('[', '').replace(']', '').split(',').map(n => Number(n)));
    }
}

// so the header format is wrong...
header[1] = header[1] << 4;
[header[2], header[3]] = [header[3], header[2]];

let sz = fs.statSync(tempFilePath).size;
if (sz % 204 === 0) {
    header.push(0);
}
else {
    header.push(204 - (sz % 204));
}

// also fragment header is wrong...
for (let f of fragments) {
    [f[1], f[2]] = [f[2], f[1]];
}

let packetsData = `/*
* PackageLicenseDeclared: Apache-2.0
* Copyright (c) 2017 ARM Limited
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef PACKETS_H
#define PACKETS_H

#include "mbed.h"

const uint8_t FAKE_PACKETS_HEADER[] = { ${header.map(n => '0x' + n.toString(16)).join(', ')} };

const uint8_t FAKE_PACKETS[][207] = {
`;

for (let f of fragments) {
    packetsData += '    { ' + f.join(', ') + ' },\n';
}
packetsData += `};

// Normally this value is retrieved from the network, but here we hard-code to do the check
uint64_t FAKE_PACKETS_CRC64_HASH = 0x${hash};

#endif
`;

fs.writeFileSync(Path.join(__dirname, '../src', 'packets.h'), packetsData, 'utf-8');

fs.unlinkSync(tempFilePath);

console.log('Done, written to packets.h')
