//
// KaraPerf.as: ActionScript performance of two-level Karatsuba multiplication
// by pts@fazekas.hu at Sun Oct  4 17:50:09 CEST 2009
//
// TODO(pts): Use Number instead of int
// TODO(pts): Use explicit int(...) coercion to avoid numbers.
// TODO(pts): Move functions outside a class?
// TODO(pts): Avoid the int and != 0 and 0 != coercions.

package {
  import flash.display.Sprite;
  import flash.events.Event;
  import flash.text.TextField;
  import flash.text.TextFormat;
  import flash.utils.getTimer;
  import flash.external.ExternalInterface;

  //mask = 32767;
  //radix = 32768;
  //bpe = 15;

  public class KaraPerf extends Sprite {

    /** Return the difference a - b (assuming it's nonnegative) of two
     * 72-word numbers, yielding a 72-word number.
     */
    internal static function sub72(a : Vector.<int>, b : Vector.<int>) : Vector.<int> {
      var i : int;
      var s : int;
      var carry : int = 0;
      var c : Vector.<int> = new Vector.<int>(72);
      for (i = 0; i < 72; ++i) {
        if (0 != (carry = int(0 > (s = a[i] - b[i] - carry)))) {
          c[i] = s + 32768;
        } else {
          c[i] = s;
        }
      }
      return c;
    }

    /** Return the difference a - b (assuming it's nonnegative) of two
     * 140-word numbers, yielding a 140-word number.
     */
    internal static function sub140(a : Vector.<int>, b : Vector.<int>) : Vector.<int> {
      var i : int;
      var s : int;
      var carry : int = 0;
      var c : Vector.<int> = new Vector.<int>(140);
      for (i = 0; i < 140; ++i) {
        if (0 != (carry = int(0 > (s = a[i] - b[i] - carry)))) {
          c[i] = s + 32768;
        } else {
          c[i] = s;
        }
      }
      return c;
    }

    /** Return the sum a + b of two
     * 36-word numbers, yielding a 36-word number, ignoring carry at the top.
     */
    internal static function add36(a : Vector.<int>, b : Vector.<int>) : Vector.<int> {
      var i : int;
      var s : int;
      var carry : int = 0;
      var c : Vector.<int> = new Vector.<int>(36);
      for (i = 0; i < 36; ++i) {
        if (0 != (carry = int(32768 <= (s = a[i] + b[i] + carry)))) {
          c[i] = s - 32768;
        } else {
          c[i] = s;
        }
      }
      return c;
    }

    /** Return the sum a + b of two
     * 70-word numbers, yielding a 70-word number, ignoring carry at the top.
     */
    internal static function add70(a : Vector.<int>, b : Vector.<int>) : Vector.<int> {
      var i : int;
      var s : int;
      var carry : int = 0;
      var c : Vector.<int> = new Vector.<int>(70);
      for (i = 0; i < 70; ++i) {
        if (0 != (carry = int(32768 <= (s = a[i] + b[i] + carry)))) {
          c[i] = s - 32768;
        } else {
          c[i] = s;
        }
      }
      return c;
    }

    /** Multiply two 36-word numbers and yield a 72-word number. */
    internal static function karamult36(a : Vector.<int>, b : Vector.<int>) : Vector.<int> {
      var m : int;
      var d : int;
      var k : int;
      var i : int;
      var bi : int;
      var c : Vector.<int> = new Vector.<int>(72);
      for (i = 0; i < 72; ++i) c[i] = 0;
      for (i = 0; i < 36; ++i) {
        if (0 != (bi = b[i])) {
          // c += b[i] * (a << (i*bpe))
          // The implementation is based on BigInt's linCombShift_().
          k= i + 36;
          for (d = 0, m = i; m < k; ++m) {
            c[m] = (d += c[m] + bi * a[m-i]) & 32767;
            d >>= 15;
          }
          for (m = k; d && m < 72; ++m) {
            c[m] = (d += c[m]) & 32767;
            d >>= 15;
          }
        }
      }
      return c;
    }

    /** Multiply two 70-word numbers and yield a 140-word number. */
    internal static function karamult70(a : Vector.<int>, b : Vector.<int>) : Vector.<int> {
      // m=35
      var x0 : Vector.<int> = a.slice(0, 35);  x0.push(0);
      var x1 : Vector.<int> = a.slice(35, 70); x1.push(0);
      var y0 : Vector.<int> = b.slice(0, 35);  y0.push(0);
      var y1 : Vector.<int> = b.slice(35, 70); y1.push(0);
      // TODO(pts): Create less arrays here.
      var z2 : Vector.<int> = karamult36(x1, y1);  // 70-word + 0 + 0
      var z0 : Vector.<int> = karamult36(x0, y0);  // 70-word + 0 + 0
      var z1 : Vector.<int> = sub72(sub72(         // 71-word + 0
               karamult36(add36(x1, x0), add36(y1, y0)), z2), z0);
      var c : Vector.<int> = new Vector.<int>(140);
      var i : int;
      var carry : int = 0;
      var s : int;
      for (i = 0; i < 35; ++i) c[i] = z0[i];
      for (; i < 70; ++i) {
        if (0 != (carry = int((s = z1[i - 35] + z0[i] + carry) >= 32768))) {
          c[i] = s - 32768;
        } else {
          c[i] = s;
        }
      }
      for (; i < 106; ++i) {  // 71 + 35
        if (0 != (carry = int((s = z1[i - 35] + z2[i - 70] + carry) >= 32768))) {
          c[i] = s - 32768;
        } else {
          c[i] = s;
        }
      }
      for (; carry; ++i) {
        if (0 != (carry = int((s = z2[i - 70] + 1) >= 32768))) {
          c[i] = s - 32768;
        } else {
          c[i] = s;
        }
      }
      for (; i < 140; ++i) c[i] = z2[i - 70];
      return c;
    }

    /** Multiply two 138-word numbers and yield a 276-word number. */
    internal static function karamult138(a : Vector.<int>, b : Vector.<int>) : Vector.<int> {
      // m=69
      var x0 : Vector.<int> = a.slice(0, 69);   x0.push(0);
      var x1 : Vector.<int> = a.slice(69, 138); x1.push(0);
      var y0 : Vector.<int> = b.slice(0, 69);   y0.push(0);
      var y1 : Vector.<int> = b.slice(69, 138); y1.push(0);
      // TODO(pts): Create less arrays here.
      var z2 : Vector.<int> = karamult70(x1, y1);  // 138-word + 0 + 0
      var z0 : Vector.<int> = karamult70(x0, y0);  // 138-word + 0 + 0
      var z1 : Vector.<int> = sub140(sub140(       // 139-word + 0
               karamult70(add70(x1, x0), add70(y1, y0)), z2), z0);
      var c : Vector.<int> = new Vector.<int>(276);
      var i : int;
      var carry : int = 0;
      var s : int;
      for (i = 0; i < 69; ++i) c[i] = z0[i];
      for (; i < 138; ++i) {
        if (0 != (carry = int((s = z1[i - 69] + z0[i] + carry) >= 32768))) {
          c[i] = s - 32768;
        } else {
          c[i] = s;
        }
      }
      for (; i < 208; ++i) {  // 139 + 69
        if (0 != (carry = int((s = z1[i - 69] + z2[i - 138] + carry) >= 32768))) {
          c[i] = s - 32768;
        } else {
          c[i] = s;
        }
      }
      for (; carry; ++i) {
        if (0 != (carry = int((s = z2[i - 138] + 1) >= 32768))) {
          c[i] = s - 32768;
        } else {
          c[i] = s;
        }
      }
      for (; i < 276; ++i) c[i] = z2[i - 138];
      return c;
    }

    internal static function newVectorInt(values : Array) : Vector.<int> {
      var i : int;
      var x : Vector.<int> = new Vector.<int>();
      for each(i in values) x.push(i);
      return x;
    }      

    public function KaraPerf() {
      var circle : Sprite = new Sprite();
      circle.graphics.beginFill(0xFF794B);
      circle.graphics.drawCircle(0, 0, 30);
      circle.graphics.endFill();
      addChild(circle);
      ExternalInterface.addCallback("myFunction", measure);
    }
    public function measure() : String {
      var i : int;
      var a : Vector.<int> = newVectorInt([23738,30205,31602,26071,28653,18058,5508,2330,13398,548,14268,19806,22136,13689,16482,26359,2475,24271,12087,24173,6298,15840,27379,13252,19951,855,14268,19806,22136,13689,16482,26359,2475,24271,12087,24173,6298,15840,27379,13252,19951,855,14268,19806,22136,13689,16482,26359,2475,24271,12087,24173,6298,15840,27379,13252,19951,855,14268,19806,22136,13689,16482,26359,2475,24271,12087,24173,6298,15840,27379,13252,19951,855,14268,19806,22136,13689,16482,26359,2475,24271,12087,24173,6298,15840,27379,13252,19951,855,14268,19806,22136,13689,16482,26359,2475,24271,12087,24173,6298,15840,27379,13252,19951,855,14268,19806,22136,13689,16482,26359,2475,24271,12087,24173,6298,15840,27379,13252,19951,855,14268,19806,22136,13689,16482,26359,2475,24271,12087,24173,6298,15840,27379,13252,19951,327,0]);
      var b : Vector.<int> = newVectorInt([9499,73,3263,18829,9291,22737,10251,22687,31367,9776,16950,6092,14459,10936,18827,6911,14102,22926,1758,17699,27753,1836,2468,1526,9925,27086,5635,5113,1477,25112,13988,5908,32547,8498,19243,15835,16651,6089,17550,21805,29686,6043,30659,26948,15957,11756,11225,27056,26009,24589,4537,28566,20826,10110,5106,5192,15717,1910,30243,22766,31766,3961,25249,22422,7536,7762,21566,26755,21968,31764,19289,21855,24446,11582,19788,10700,3510,32041,5755,12793,26968,3011,340,8694,11329,11325,13623,25759,24173,28663,373,20387,7522,11778,25005,32330,25467,22697,25962,29449,24708,31707,6139,21679,3211,16090,6388,10564,1930,2417,5793,11149,21265,24341,7278,30769,17711,16855,69,31754,13263,10309,16624,11143,27808,1983,20576,17584,13897,2798,11385,20421,2026,9562,12889,29797,20132,501,0]);
      //var m : Vector.<int> = newVectorInt([9499,73,3263,18829,9291,22737,10251,22687,31367,9776,16950,6092,14459,10936,18827,6911,14102,22926,1758,17699,27753,1836,2468,1526,9925,27086,5635,5113,1477,25112,13988,5908,32547,8498,19243,15835,16651,6089,17550,21805,29686,6043,30659,26948,15957,11756,11225,27056,26009,24589,4537,28566,20826,10110,5106,5192,15717,1910,30243,22766,31766,3961,25249,22422,7536,7762,21566,26755,21968,31764,19289,21855,24446,11582,19788,10700,3510,32041,5755,12793,26968,3011,340,8694,11329,11325,13623,25759,24173,28663,373,20387,7522,11778,25005,32330,25467,22697,25962,29449,24708,31707,6139,21679,3211,16090,6388,10564,1930,2417,5793,11149,21265,24341,7278,30769,17711,16855,69,31754,13263,10309,16624,11143,27808,1983,20576,17584,13897,2798,11385,20421,2026,9562,12889,29797,28580,381,0]);
      var ab : Vector.<int> = newVectorInt([10654,3738,11610,12716,24071,12381,19463,14150,12731,21509,7689,5406,23086,13095,30086,14100,7767,5930,2276,18689,21672,22344,17738,14171,4271,28443,19582,31343,21772,27657,18103,23945,13223,25376,24665,19180,11546,31474,3345,16569,16752,14464,28889,15934,23589,23468,8426,24253,26090,17477,14521,4967,24629,26336,18163,15754,22627,4370,8510,18885,1175,4857,21745,23457,123,15424,20017,18600,15435,29856,24483,31714,9764,8769,27677,15626,1189,1718,5759,16387,22132,6878,18900,27102,11383,9209,20602,18540,7983,31998,9208,5215,5595,2236,23077,16292,27859,12308,32735,13319,2661,27681,25401,19736,23568,9599,23210,9072,30366,27259,10011,22935,7199,23816,26164,28806,30488,18512,17855,7462,6269,20110,12810,29506,710,5519,386,31824,2669,2543,6653,10291,1734,4469,29443,2475,2517,2801,7951,18555,20613,25029,30652,22070,25135,13571,6152,17294,17863,7369,15736,12652,13580,13937,12771,27501,26337,1374,17829,5661,26050,20153,20440,14979,31795,17895,22624,12586,16476,11747,20723,16093,21032,24684,8161,19901,7553,32370,185,8630,190,28121,5941,20728,21291,3400,7925,24654,12614,12468,15250,30440,28212,22160,15410,25423,11447,21719,11679,27935,53,25462,20193,21617,30221,27884,18283,8930,28979,32747,1845,5606,20714,24255,18415,29318,14463,7607,10764,28989,5664,22578,1856,13376,14735,27059,1405,30151,14788,9011,19308,10415,26612,25449,15652,20919,27097,3569,5718,3186,14860,2085,12046,11012,16107,17769,2868,8409,30020,5002,1563,2082,9209,8084,4537,1069,27429,27703,18950,16835,5469,26196,8773,8134,30135,3604,32222,6727,30132,12910,4161,6875,9091,10765,493,5]);
      trace(getTimer() + ': measure');

      var c : Vector.<int> = karamult138(a, b);
      if (c.join(',') != ab.join(',')) return "mismatch";
      var date1 : Number = getTimer();
      for (i = 0; i < 1000; ++i) {
        c = karamult138(a, b);
      }
      var date2 : Number = getTimer();
      return (date2 - date1) + ' = MILLISEC(ActionScript3,Flash10)'
    }
  }
}
