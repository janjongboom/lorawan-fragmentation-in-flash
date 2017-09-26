const fs = require('fs');
const Path = require('path');
const execSync = require('child_process').execSync;

const bin = Path.resolve(process.argv[2]);

const infile = execSync('python ' + Path.join(__dirname, 'encode_file.py') + ' ' + bin + ' 204 20').toString('utf-8').split('\n');

// compile crc64 app
execSync(`gcc ${Path.join(__dirname, 'calculate-crc64', 'main.cpp')} -o ${Path.join(__dirname, 'calculate-crc64', 'crc64')}`);
const hash = execSync(`${Path.join(__dirname, 'calculate-crc64', 'crc64')} ${bin}`).toString('utf-8');
console.log('hash is ', hash);

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

let sz = fs.statSync(bin).size;
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

let data = `/*
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

uint8_t FAKE_PACKETS_HEADER[] = { ${header.map(n => '0x' + n.toString(16)).join(', ')} };

uint8_t FAKE_PACKETS[][207] = {
`;

for (let f of fragments) {
    data += '    { ' + f.join(', ') + ' },\n';
}
data += `};

uint64_t FAKE_PACKETS_HASH = 0x${hash};

#endif
`;

fs.writeFileSync(outfile, data, 'utf-8');
