/**
 * Generates a small file to check if the fragmentation stuff worked
 * Usage: node generate-bin.js outfile
 *
 * Output file is in the form of
 * 0x00 (20 times)
 * 0x01 (20 times)
 * 0x02 (20 times)
 * 0x03 (20 times)
 * ... until 0xff
 *
 * Then run $ python encode_file.py test-file.bin 204 50 > fragments.txt to get the fragments
 */

const fs = require('fs');
const outfile = process.argv[2];

let blah = [];
for (let ix = 0; ix <= 255; ix++) {
    for (let j = 0; j < 20; j++) {
        blah.push(ix);
    }
}

// 25 bytes padding
for (let ix = 0; ix < 25; ix++) {
    blah.push(0);
}

let buffer = Buffer.from(blah);
fs.writeFileSync(outfile, buffer);

