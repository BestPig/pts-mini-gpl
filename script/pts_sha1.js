//
// pts_sha1.js: fast SHA1 implementation in JavaScript
// by pts@fazekas.hu at Fri Jun 19 23:11:39 CEST 2009
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

function sha1Hash(msg) {
  var hexBytes = '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff';
  var a  = msg.length;
  var N = ((a >> 2) + 18) >> 4;
  var M = new Array(N);
  var mo = 0;
  var i, j;
  for (i = 0; i < N; i++) {
    M[i] = new Array(16);
    for (j = 0; j < 16; j++) {
      M[i][j] = (msg.charCodeAt(mo) << 24) | (msg.charCodeAt(mo + 1)<<16) | 
                (msg.charCodeAt(mo + 2) << 8) | (msg.charCodeAt(mo + 3));
      mo += 4;
    }
  }
  M[a >> 6][(a >> 2) & 15] |= 0x80000000 >>> (8 * (a & 3));
  while (a > 0x1fffffff) { a -= 0x20000000; ++M[N-1][14]; }
  M[N-1][15] = (a << 3) & 0xffffffff;
  var H0 = 0x67452301;
  var H1 = 0xefcdab89;
  var H2 = 0x98badcfe;
  var H3 = 0x10325476;
  var H4 = 0xc3d2e1f0;
  var W = new Array(80); var b, c, d, e, t, s;
  for (i=0; i<N; i++) {
    for (t=0;  t<16; t++) W[t] = M[i][t];
    for (; t<80; t++) {
      j = W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16];
      W[t] = (j << 1) | (j >>> 31);
    }
    a = H0; b = H1; c = H2; d = H3; e = H4;
    for (t = 0; t < 20; t++) {  // Ch()
      j = (((a << 5) | (a >>> 27)) + ((b & c) ^ (~b & d)) + e + 0x5a827999 + W[t]) & 0xffffffff;
      e = d;
      d = c;
      c = (b << 30) | (b >>> 2);
      b = a;
      a = j;
    }
    for (; t < 40; t++) {  // Parity()
      j = (((a << 5) | (a >>> 27)) + (b ^ c ^ d) + e + 0x6ed9eba1 + W[t]) & 0xffffffff;
      e = d;
      d = c;
      c = (b << 30) | (b >>> 2);
      b = a;
      a = j;
    }
    for (; t < 60; t++) {  // Maj()
      j = (((a << 5) | (a >>> 27)) + ((b & c) ^ (b & d) ^ (c & d)) + e + 0x8f1bbcdc + W[t]) & 0xffffffff;
      e = d;
      d = c;
      c = (b << 30) | (b >>> 2);
      b = a;
      a = j;
    }
    for (; t < 80; t++) {  // Parity()
      j = (((a << 5) | (a >>> 27)) + (b ^ c ^ d) + e + 0xca62c1d6 + W[t]) & 0xffffffff;
      e = d;
      d = c;
      c = (b << 30) | (b >>> 2);
      b = a;
      a = j;
    }
    H0 = (H0 +a) & 0xffffffff;
    H1 = (H1 +b) & 0xffffffff;
    H2 = (H2 +c) & 0xffffffff;
    H3 = (H3 +d) & 0xffffffff;
    H4 = (H4 +e) & 0xffffffff;
  }
  return [hexBytes.substr((H0 >>> 23) & 510, 2),
          hexBytes.substr((H0 >>> 15) & 510, 2),
          hexBytes.substr((H0 >>> 7) & 510, 2),
          hexBytes.substr((H0 & 255) << 1, 2),
          hexBytes.substr((H1 >>> 23) & 510, 2),
          hexBytes.substr((H1 >>> 15) & 510, 2),
          hexBytes.substr((H1 >>> 7) & 510, 2),
          hexBytes.substr((H1 & 255) << 1, 2),
          hexBytes.substr((H2 >>> 23) & 510, 2),
          hexBytes.substr((H2 >>> 15) & 510, 2),
          hexBytes.substr((H2 >>> 7) & 510, 2),
          hexBytes.substr((H2 & 255) << 1, 2),
          hexBytes.substr((H3 >>> 23) & 510, 2),
          hexBytes.substr((H3 >>> 15) & 510, 2),
          hexBytes.substr((H3 >>> 7) & 510, 2),
          hexBytes.substr((H3 & 255) << 1, 2),
          hexBytes.substr((H4 >>> 23) & 510, 2),
          hexBytes.substr((H4 >>> 15) & 510, 2),
          hexBytes.substr((H4 >>> 7) & 510, 2),
          hexBytes.substr((H4 & 255) << 1, 2),
         ].join('');
}

if (sha1Hash('Hello, World!') != '0a0a9f2a6772942557ab5355d76af442f8f65e01') alert('bad hello SHA1');
