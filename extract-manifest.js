var bla = '30 82 01 27 30 81 89 0a 01 00 30 81 83 0a 01 01 02 04 5b 04 42 57 04 10 fa 6b 4a 53 d5 ad 5f df be 9d e6 63 e4 d4 1f fe 04 10 e6 1b 2b ae f9 65 5a d0 86 04 2c 2f 06 ed 2e a0 04 00 04 10 1f e5 89 e1 ca 56 e9 63 fe d4 37 42 e3 cc b7 ae 04 00 01 01 ff 0a 01 02 30 00 30 00 30 34 0a 01 01 0c 07 64 65 66 61 75 6c 74 30 26 04 20 98 88 4c 52 0d cf 8b 5c f0 30 fe 63 f9 fa 71 c5 94 8b 12 e0 26 5e 6f 47 b1 86 cc f4 c4 ad 44 0e 02 02 31 c8 30 81 98 04 20 a2 22 80 07 8d 90 43 65 0a cd f5 66 89 22 f9 dc a2 7b 75 72 77 81 27 07 8c 13 f9 a3 d5 52 93 81 30 74 30 72 04 48 30 46 02 21 00 db 91 5c b7 76 3f 26 a3 9f 65 9b c9 d6 02 84 1c aa b2 50 d2 79 0e 48 2e 9e 63 0d 26 71 dd ba a8 02 21 00 82 f0 0b 7e 28 93 96 0d 1d f1 cd 42 2f 08 dc 88 04 24 9d 52 59 9e 1c d1 d3 82 9e bd 0d cd c7 8e 30 26 30 24 04 20 d8 50 c9 63 b4 9d c7 1f 55 93 55 81 c2 8d ad 1c ba 5d a5 d6 0e c9 1b ea 88 9d b2 2c a9 7b 77 6c 0c 00';

bla = bla.split(' ').map(c=>parseInt(c, 16));

require('fs').writeFileSync('./manifest', Buffer.from(bla));