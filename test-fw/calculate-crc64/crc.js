/* Redis uses the CRC64 variant with "Jones" coefficients and init value of 0.
 *
 * Specification of this CRC64 variant follows:
 * Name: crc-64-jones
 * Width: 64 bites
 * Poly: 0xad93d23594c935a9
 * Reflected In: True
 * Xor_In: 0xffffffffffffffff
 * Reflected_Out: True
 * Xor_Out: 0x0
 * Check("123456789"): 0xe9c6d914c4b8d9ca
 *
 * Copyright (c) 2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. */

const bigInt = require('./bigint');
const fs = require('fs');

var crc64_tab = [
    bigInt('0000000000000000', 16), bigInt('7ad870c830358979', 16),
    bigInt('f5b0e190606b12f2', 16), bigInt('8f689158505e9b8b', 16),
    bigInt('c038e5739841b68f', 16), bigInt('bae095bba8743ff6', 16),
    bigInt('358804e3f82aa47d', 16), bigInt('4f50742bc81f2d04', 16),
    bigInt('ab28ecb46814fe75', 16), bigInt('d1f09c7c5821770c', 16),
    bigInt('5e980d24087fec87', 16), bigInt('24407dec384a65fe', 16),
    bigInt('6b1009c7f05548fa', 16), bigInt('11c8790fc060c183', 16),
    bigInt('9ea0e857903e5a08', 16), bigInt('e478989fa00bd371', 16),
    bigInt('7d08ff3b88be6f81', 16), bigInt('07d08ff3b88be6f8', 16),
    bigInt('88b81eabe8d57d73', 16), bigInt('f2606e63d8e0f40a', 16),
    bigInt('bd301a4810ffd90e', 16), bigInt('c7e86a8020ca5077', 16),
    bigInt('4880fbd87094cbfc', 16), bigInt('32588b1040a14285', 16),
    bigInt('d620138fe0aa91f4', 16), bigInt('acf86347d09f188d', 16),
    bigInt('2390f21f80c18306', 16), bigInt('594882d7b0f40a7f', 16),
    bigInt('1618f6fc78eb277b', 16), bigInt('6cc0863448deae02', 16),
    bigInt('e3a8176c18803589', 16), bigInt('997067a428b5bcf0', 16),
    bigInt('fa11fe77117cdf02', 16), bigInt('80c98ebf2149567b', 16),
    bigInt('0fa11fe77117cdf0', 16), bigInt('75796f2f41224489', 16),
    bigInt('3a291b04893d698d', 16), bigInt('40f16bccb908e0f4', 16),
    bigInt('cf99fa94e9567b7f', 16), bigInt('b5418a5cd963f206', 16),
    bigInt('513912c379682177', 16), bigInt('2be1620b495da80e', 16),
    bigInt('a489f35319033385', 16), bigInt('de51839b2936bafc', 16),
    bigInt('9101f7b0e12997f8', 16), bigInt('ebd98778d11c1e81', 16),
    bigInt('64b116208142850a', 16), bigInt('1e6966e8b1770c73', 16),
    bigInt('8719014c99c2b083', 16), bigInt('fdc17184a9f739fa', 16),
    bigInt('72a9e0dcf9a9a271', 16), bigInt('08719014c99c2b08', 16),
    bigInt('4721e43f0183060c', 16), bigInt('3df994f731b68f75', 16),
    bigInt('b29105af61e814fe', 16), bigInt('c849756751dd9d87', 16),
    bigInt('2c31edf8f1d64ef6', 16), bigInt('56e99d30c1e3c78f', 16),
    bigInt('d9810c6891bd5c04', 16), bigInt('a3597ca0a188d57d', 16),
    bigInt('ec09088b6997f879', 16), bigInt('96d1784359a27100', 16),
    bigInt('19b9e91b09fcea8b', 16), bigInt('636199d339c963f2', 16),
    bigInt('df7adabd7a6e2d6f', 16), bigInt('a5a2aa754a5ba416', 16),
    bigInt('2aca3b2d1a053f9d', 16), bigInt('50124be52a30b6e4', 16),
    bigInt('1f423fcee22f9be0', 16), bigInt('659a4f06d21a1299', 16),
    bigInt('eaf2de5e82448912', 16), bigInt('902aae96b271006b', 16),
    bigInt('74523609127ad31a', 16), bigInt('0e8a46c1224f5a63', 16),
    bigInt('81e2d7997211c1e8', 16), bigInt('fb3aa75142244891', 16),
    bigInt('b46ad37a8a3b6595', 16), bigInt('ceb2a3b2ba0eecec', 16),
    bigInt('41da32eaea507767', 16), bigInt('3b024222da65fe1e', 16),
    bigInt('a2722586f2d042ee', 16), bigInt('d8aa554ec2e5cb97', 16),
    bigInt('57c2c41692bb501c', 16), bigInt('2d1ab4dea28ed965', 16),
    bigInt('624ac0f56a91f461', 16), bigInt('1892b03d5aa47d18', 16),
    bigInt('97fa21650afae693', 16), bigInt('ed2251ad3acf6fea', 16),
    bigInt('095ac9329ac4bc9b', 16), bigInt('7382b9faaaf135e2', 16),
    bigInt('fcea28a2faafae69', 16), bigInt('8632586aca9a2710', 16),
    bigInt('c9622c4102850a14', 16), bigInt('b3ba5c8932b0836d', 16),
    bigInt('3cd2cdd162ee18e6', 16), bigInt('460abd1952db919f', 16),
    bigInt('256b24ca6b12f26d', 16), bigInt('5fb354025b277b14', 16),
    bigInt('d0dbc55a0b79e09f', 16), bigInt('aa03b5923b4c69e6', 16),
    bigInt('e553c1b9f35344e2', 16), bigInt('9f8bb171c366cd9b', 16),
    bigInt('10e3202993385610', 16), bigInt('6a3b50e1a30ddf69', 16),
    bigInt('8e43c87e03060c18', 16), bigInt('f49bb8b633338561', 16),
    bigInt('7bf329ee636d1eea', 16), bigInt('012b592653589793', 16),
    bigInt('4e7b2d0d9b47ba97', 16), bigInt('34a35dc5ab7233ee', 16),
    bigInt('bbcbcc9dfb2ca865', 16), bigInt('c113bc55cb19211c', 16),
    bigInt('5863dbf1e3ac9dec', 16), bigInt('22bbab39d3991495', 16),
    bigInt('add33a6183c78f1e', 16), bigInt('d70b4aa9b3f20667', 16),
    bigInt('985b3e827bed2b63', 16), bigInt('e2834e4a4bd8a21a', 16),
    bigInt('6debdf121b863991', 16), bigInt('1733afda2bb3b0e8', 16),
    bigInt('f34b37458bb86399', 16), bigInt('8993478dbb8deae0', 16),
    bigInt('06fbd6d5ebd3716b', 16), bigInt('7c23a61ddbe6f812', 16),
    bigInt('3373d23613f9d516', 16), bigInt('49aba2fe23cc5c6f', 16),
    bigInt('c6c333a67392c7e4', 16), bigInt('bc1b436e43a74e9d', 16),
    bigInt('95ac9329ac4bc9b5', 16), bigInt('ef74e3e19c7e40cc', 16),
    bigInt('601c72b9cc20db47', 16), bigInt('1ac40271fc15523e', 16),
    bigInt('5594765a340a7f3a', 16), bigInt('2f4c0692043ff643', 16),
    bigInt('a02497ca54616dc8', 16), bigInt('dafce7026454e4b1', 16),
    bigInt('3e847f9dc45f37c0', 16), bigInt('445c0f55f46abeb9', 16),
    bigInt('cb349e0da4342532', 16), bigInt('b1eceec59401ac4b', 16),
    bigInt('febc9aee5c1e814f', 16), bigInt('8464ea266c2b0836', 16),
    bigInt('0b0c7b7e3c7593bd', 16), bigInt('71d40bb60c401ac4', 16),
    bigInt('e8a46c1224f5a634', 16), bigInt('927c1cda14c02f4d', 16),
    bigInt('1d148d82449eb4c6', 16), bigInt('67ccfd4a74ab3dbf', 16),
    bigInt('289c8961bcb410bb', 16), bigInt('5244f9a98c8199c2', 16),
    bigInt('dd2c68f1dcdf0249', 16), bigInt('a7f41839ecea8b30', 16),
    bigInt('438c80a64ce15841', 16), bigInt('3954f06e7cd4d138', 16),
    bigInt('b63c61362c8a4ab3', 16), bigInt('cce411fe1cbfc3ca', 16),
    bigInt('83b465d5d4a0eece', 16), bigInt('f96c151de49567b7', 16),
    bigInt('76048445b4cbfc3c', 16), bigInt('0cdcf48d84fe7545', 16),
    bigInt('6fbd6d5ebd3716b7', 16), bigInt('15651d968d029fce', 16),
    bigInt('9a0d8ccedd5c0445', 16), bigInt('e0d5fc06ed698d3c', 16),
    bigInt('af85882d2576a038', 16), bigInt('d55df8e515432941', 16),
    bigInt('5a3569bd451db2ca', 16), bigInt('20ed197575283bb3', 16),
    bigInt('c49581ead523e8c2', 16), bigInt('be4df122e51661bb', 16),
    bigInt('3125607ab548fa30', 16), bigInt('4bfd10b2857d7349', 16),
    bigInt('04ad64994d625e4d', 16), bigInt('7e7514517d57d734', 16),
    bigInt('f11d85092d094cbf', 16), bigInt('8bc5f5c11d3cc5c6', 16),
    bigInt('12b5926535897936', 16), bigInt('686de2ad05bcf04f', 16),
    bigInt('e70573f555e26bc4', 16), bigInt('9ddd033d65d7e2bd', 16),
    bigInt('d28d7716adc8cfb9', 16), bigInt('a85507de9dfd46c0', 16),
    bigInt('273d9686cda3dd4b', 16), bigInt('5de5e64efd965432', 16),
    bigInt('b99d7ed15d9d8743', 16), bigInt('c3450e196da80e3a', 16),
    bigInt('4c2d9f413df695b1', 16), bigInt('36f5ef890dc31cc8', 16),
    bigInt('79a59ba2c5dc31cc', 16), bigInt('037deb6af5e9b8b5', 16),
    bigInt('8c157a32a5b7233e', 16), bigInt('f6cd0afa9582aa47', 16),
    bigInt('4ad64994d625e4da', 16), bigInt('300e395ce6106da3', 16),
    bigInt('bf66a804b64ef628', 16), bigInt('c5bed8cc867b7f51', 16),
    bigInt('8aeeace74e645255', 16), bigInt('f036dc2f7e51db2c', 16),
    bigInt('7f5e4d772e0f40a7', 16), bigInt('05863dbf1e3ac9de', 16),
    bigInt('e1fea520be311aaf', 16), bigInt('9b26d5e88e0493d6', 16),
    bigInt('144e44b0de5a085d', 16), bigInt('6e963478ee6f8124', 16),
    bigInt('21c640532670ac20', 16), bigInt('5b1e309b16452559', 16),
    bigInt('d476a1c3461bbed2', 16), bigInt('aeaed10b762e37ab', 16),
    bigInt('37deb6af5e9b8b5b', 16), bigInt('4d06c6676eae0222', 16),
    bigInt('c26e573f3ef099a9', 16), bigInt('b8b627f70ec510d0', 16),
    bigInt('f7e653dcc6da3dd4', 16), bigInt('8d3e2314f6efb4ad', 16),
    bigInt('0256b24ca6b12f26', 16), bigInt('788ec2849684a65f', 16),
    bigInt('9cf65a1b368f752e', 16), bigInt('e62e2ad306bafc57', 16),
    bigInt('6946bb8b56e467dc', 16), bigInt('139ecb4366d1eea5', 16),
    bigInt('5ccebf68aecec3a1', 16), bigInt('2616cfa09efb4ad8', 16),
    bigInt('a97e5ef8cea5d153', 16), bigInt('d3a62e30fe90582a', 16),
    bigInt('b0c7b7e3c7593bd8', 16), bigInt('ca1fc72bf76cb2a1', 16),
    bigInt('45775673a732292a', 16), bigInt('3faf26bb9707a053', 16),
    bigInt('70ff52905f188d57', 16), bigInt('0a2722586f2d042e', 16),
    bigInt('854fb3003f739fa5', 16), bigInt('ff97c3c80f4616dc', 16),
    bigInt('1bef5b57af4dc5ad', 16), bigInt('61372b9f9f784cd4', 16),
    bigInt('ee5fbac7cf26d75f', 16), bigInt('9487ca0fff135e26', 16),
    bigInt('dbd7be24370c7322', 16), bigInt('a10fceec0739fa5b', 16),
    bigInt('2e675fb4576761d0', 16), bigInt('54bf2f7c6752e8a9', 16),
    bigInt('cdcf48d84fe75459', 16), bigInt('b71738107fd2dd20', 16),
    bigInt('387fa9482f8c46ab', 16), bigInt('42a7d9801fb9cfd2', 16),
    bigInt('0df7adabd7a6e2d6', 16), bigInt('772fdd63e7936baf', 16),
    bigInt('f8474c3bb7cdf024', 16), bigInt('829f3cf387f8795d', 16),
    bigInt('66e7a46c27f3aa2c', 16), bigInt('1c3fd4a417c62355', 16),
    bigInt('935745fc4798b8de', 16), bigInt('e98f353477ad31a7', 16),
    bigInt('a6df411fbfb21ca3', 16), bigInt('dc0731d78f8795da', 16),
    bigInt('536fa08fdfd90e51', 16), bigInt('29b7d047efec8728', 16),
];

module.exports = function(buff) {
    let crc = bigInt(0);
    let ix = 0;

    for (let byte of buff) {
        // console.log(`crc=${crc}, byte=${byte}`);
        var tabix = crc.and(0xff).xor(bigInt(byte));
        crc = crc64_tab[tabix].xor(crc.shiftRight(8));
        // console.log(`j=${ix}, byte=${byte.toString(16)}, crc=${crc}, tabix=${tabix}, tabv=${crc64_tab[tabix].toString(16)}`);
        // if (++ix > 5) process.exit(1);
    }

    return crc.toString(16);
}
