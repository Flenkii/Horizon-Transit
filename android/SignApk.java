import com.android.apksig.ApkSigner;
import com.android.apksig.ApkVerifier;

import java.io.File;
import java.io.FileInputStream;
import java.security.KeyStore;
import java.security.PrivateKey;
import java.security.cert.X509Certificate;
import java.util.Collections;

/** Signs the app (APK signature v2: enough from Android 7.0, the app's minimum) with apksig and checks the result: SignApk in out keystore.p12 pass minSdk */
public class SignApk {
    public static void main(String[] a) throws Exception {
        KeyStore ks = KeyStore.getInstance("PKCS12");
        FileInputStream in = new FileInputStream(a[2]);
        ks.load(in, a[3].toCharArray());
        in.close();
        String alias = ks.aliases().nextElement();
        PrivateKey key = (PrivateKey) ks.getKey(alias, a[3].toCharArray());
        X509Certificate cert = (X509Certificate) ks.getCertificate(alias);
        ApkSigner.SignerConfig cfg = new ApkSigner.SignerConfig.Builder("HTRANSIT", key, Collections.singletonList(cert)).build();
        new ApkSigner.Builder(Collections.singletonList(cfg)).setInputApk(new File(a[0])).setOutputApk(new File(a[1]))
                .setMinSdkVersion(Integer.parseInt(a[4])).setV1SigningEnabled(false).setV2SigningEnabled(true).build().sign();
        ApkVerifier.Result r = new ApkVerifier.Builder(new File(a[1])).setMinCheckedPlatformVersion(Integer.parseInt(a[4])).build().verify();
        System.out.println("signature ok: " + r.isVerified() + " (v1 " + r.isVerifiedUsingV1Scheme() + ", v2 " + r.isVerifiedUsingV2Scheme() + ")");
        for (Object e : r.getErrors()) System.out.println("error: " + e);
        if (!r.isVerified()) System.exit(1);
    }
}
