// javac -target 1.2 -source 1.2 exponentiation_test.java 
import java.math.BigInteger;

public class karaperf {
  public static void main(String[] args) {
    BigInteger a = new BigInteger("a3cdef6789abcdef0189abcdabcdef6789abcdef0189abcd6789abcdef01abcdef6789abcdef0189abcdabcdef6789abcdef0189abcd6789abcdef01abcdef6789abcdef0189abcdabcdef6789abcdef0189abcd6789abcdef01abcdef6789abcdef0189abcdabcdef6789abcdef0189abcd6789abcdef01abcdef6789abcdef0189abcdabcdef6789abcdef0189abcd6789abcdef01abcdef6789abcdef0189abcdabcdef6789abcdef0189abcd6789abcdef01abcdef6789abcdef0189abcdabcdef6789abcdef0189abcd6789abcdef01abcdef6789abcdef0189abcdabcdef6789abcdef0189abcd6789abcdef01123456123456123456fedcbafedcbafedcba", 16);
    BigInteger b = new BigInteger("facea4e8cac9652ad07ea9f8ab1e45773649896141803dfeca0570f03c1422b3cff81401160ebc52ff06271baf8ad311571a5a844b8878a528863d1f6d0c8ba95e5fefdede084e61395aac54e37bfc9586b57011d629f4605d77fbde6dc93ed4dd61eac4143ec05505e1e95863f259efe948db65399353169f5f7eaabf2d67e0a55d0d10750f8f291d70af2d8a847bcfc16b1ddd88c3bb3d6528904fc93bf515adf2c46e7006e599d360af656f63e55d289df0cbcdf3f6aa5b1238be4c10b7bb72cad0997f232e28da9310c05c527f2580f4e726c50bec26903966c698a461b7acc7371635ff262d55c387b2f9908d93187a87b13ea02ec68a44b931a32fc024a51b", 16);
    BigInteger v = null;
    System.out.print("benchmarking\n");
    for (int i = 0; i < 100000; ++i) {
      v = a.multiply(b);
    }
    System.out.println(v);
  }
}
