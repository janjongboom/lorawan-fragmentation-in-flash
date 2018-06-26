const fs = require('fs');
const Path = require('path');
const execSync = require('child_process').execSync;
const UUID = require('uuid-1345');
const deviceId = require('./certs/device-ids');
const crc64 = require('./calculate-crc64/crc');

let manufacturerUUID = new UUID(deviceId['manufacturer-uuid']).toBuffer();
let deviceClassUUID = new UUID(deviceId['device-class-uuid']).toBuffer();

// diff info contains (bool is_diff, 3 bytes for the size of the *old* firmware)
let isDiffBuffer = Buffer.from([ 0, 0, 0, 0 ]);

const binaryPath = Path.resolve(process.argv[2]);
const tempFilePath = Path.join(__dirname, 'temp.bin');

// now we need to create a signature...
let signature = execSync(`openssl dgst -sha256 -sign ${Path.join(__dirname, 'certs', 'update.key')} ${binaryPath}`);
console.log('Signed signature is', signature.toString('hex'));

let sigLength = Buffer.from([ signature.length ]);

if (signature.length === 70) {
    signature = Buffer.concat([ signature, Buffer.from([ 0, 0 ]) ]);
}
else if (signature.length === 71) {
    signature = Buffer.concat([ signature, Buffer.from([ 0 ]) ]);
}

let manifest = Buffer.concat([ sigLength, signature, manufacturerUUID, deviceClassUUID, isDiffBuffer ]);

// now make a temp file which contains bin + signature + class IDs + if it's a diff or not
fs.writeFileSync(tempFilePath, Buffer.concat([ fs.readFileSync(binaryPath), manifest ]));

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
* Copyright (c) 2018 ARM Limited
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
