#!/usr/bin/env python3
"""Builds the Android app (out/HorizonTransit.apk) without Android Studio or the Android SDK.

  python android/build_apk.py

What it needs (all from Maven Central, put into android/tools/ the first time):
  android-all (Robolectric's Android 14 classes, only to compile against), dalvik-dx (Java classes -> classes.dex),
  apksig (signs the APK with signature scheme v2, which Android 7.0 and newer check). And a JDK (javac, java, keytool).
The binary AndroidManifest.xml and resources.arsc (only the app icon) are written here by hand: their formats are
Android's ResChunk structures (frameworks/base/libs/androidfw/include/androidfw/ResourceTypes.h).
The page the app shows is ../app/index.html, the same the mod serves.
The signing key is android/release.p12: keep it. An update of the app must be signed with the same key, and it
doesn't go on GitHub (.gitignore).
"""
import os, shutil, struct, subprocess, sys, urllib.request, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
TOOLS = os.path.join(HERE, "tools")
BUILD = os.path.join(HERE, "build")
OUT = os.path.join(ROOT, "out", "HorizonTransit.apk")
PACKAGE = "io.github.flenkii.horizontransit"
VERSION_NAME = "0.13.0"
VERSION_CODE = 23
MIN_SDK, TARGET_SDK = 24, 34
KEYSTORE = os.path.join(HERE, "release.p12")
KEY_PASS = "horizontransit"   # protects nothing secret: only that nobody else can sign updates without the file

MAVEN = "https://repo.maven.apache.org/maven2/"
JARS = {
    "android-all.jar": "org/robolectric/android-all/14-robolectric-10818077/android-all-14-robolectric-10818077.jar",
    "dx.jar": "com/jakewharton/android/repackaged/dalvik-dx/16.0.1/dalvik-dx-16.0.1.jar",
    "apksig.jar": "com/android/tools/build/apksig/2.3.0/apksig-2.3.0.jar",
}

# android:attr ids (android.R.attr, checked against Android 14's R$attr)
A = {"theme": 0x01010000, "label": 0x01010001, "icon": 0x01010002, "name": 0x01010003, "exported": 0x01010010,
     "screenOrientation": 0x0101001E, "configChanges": 0x0101001F, "minSdkVersion": 0x0101020C,
     "versionCode": 0x0101021B, "versionName": 0x0101021C, "windowSoftInputMode": 0x0101022B,
     "targetSdkVersion": 0x01010270, "hardwareAccelerated": 0x010102D3, "usesCleartextTraffic": 0x010104EC}
THEME = 16974382  # @android:style/Theme.Material.NoActionBar
ICON = 0x7F010000  # our own: drawable/icon (resources.arsc below)
NS = "http://schemas.android.com/apk/res/android"


def tools():
    os.makedirs(TOOLS, exist_ok=True)
    for name, path in JARS.items():
        dst = os.path.join(TOOLS, name)
        if not os.path.exists(dst):
            print("downloading", name)
            urllib.request.urlretrieve(MAVEN + path, dst + ".part")
            os.replace(dst + ".part", dst)


def run(cmd, **kw):
    print(">", " ".join(cmd))
    subprocess.check_call(cmd, **kw)


# ------------------------------------------------------------------ binary XML (AXML)

def pad4(b):
    return b + b"\0" * (-len(b) % 4)


def string_pool(strings):
    """a ResStringPool in UTF-16"""
    offs, data = [], b""
    for s in strings:
        offs.append(len(data))
        u = s.encode("utf-16-le")
        data += struct.pack("<H", len(s)) + u + b"\0\0"
    data = pad4(data)
    header = 28
    start = header + 4 * len(strings)
    body = b"".join(struct.pack("<I", o) for o in offs) + data
    return struct.pack("<HHIIIIII", 0x0001, header, header + len(body), len(strings), 0, 0, start, 0) + body


class Axml:
    """a little writer: element(name, attrs, children); attrs: (ns or None, name, value)"""

    def __init__(self):
        self.attr_names = []   # android: attributes first (their ids go in the resource map, in the same order)
        self.strings = []

    def s(self, text):
        if text not in self.strings:
            self.strings.append(text)
        return self.strings.index(text)

    def build(self, root):
        # the android attributes' names first, in a fixed order
        names = []

        def walk(e):
            for ns, n, v in e[1]:
                if ns and n not in names:
                    names.append(n)
            for c in e[2]:
                walk(c)
        walk(root)
        self.strings = list(names)
        body = []
        line = [1]
        ns_prefix, ns_uri = self.s("android"), self.s(NS)

        def node(e):
            tag, attrs, kids = e
            attrs = sorted(attrs, key=lambda a: A[a[1]] if a[0] else -1)  # no-namespace ones (package) first, then by id
            raw = b""
            for ns, n, v in attrs:
                nsi = ns_uri if ns else 0xFFFFFFFF
                ni = self.s(n)
                if isinstance(v, bool):
                    raw += struct.pack("<IIIHBBI", nsi, ni, 0xFFFFFFFF, 8, 0, 0x12, 0xFFFFFFFF if v else 0)
                elif isinstance(v, int):
                    kind = 0x01 if v >= 0x01000000 else 0x11 if n == "configChanges" or n == "windowSoftInputMode" else 0x10
                    raw += struct.pack("<IIIHBBI", nsi, ni, 0xFFFFFFFF, 8, 0, kind, v & 0xFFFFFFFF)
                else:
                    si = self.s(v)
                    raw += struct.pack("<IIIHBBI", nsi, ni, si, 8, 0, 0x03, si)
            head = struct.pack("<IIIIHHHHHH", line[0], 0xFFFFFFFF, 0xFFFFFFFF, self.s(tag), 20, 20, len(attrs), 0, 0, 0)
            body.append(struct.pack("<HHI", 0x0102, 16, 8 + len(head) + len(raw)) + head + raw)
            line[0] += 1
            for k in kids:
                node(k)
            body.append(struct.pack("<HHIIIII", 0x0103, 16, 24, line[0], 0xFFFFFFFF, 0xFFFFFFFF, self.s(tag)))
            line[0] += 1

        ns_start = struct.pack("<HHIIIII", 0x0100, 16, 24, 1, 0xFFFFFFFF, ns_prefix, ns_uri)
        node(root)
        ns_end = struct.pack("<HHIIIII", 0x0101, 16, 24, line[0], 0xFFFFFFFF, ns_prefix, ns_uri)
        pool = string_pool(self.strings)
        resmap = struct.pack("<HHI", 0x0180, 8, 8 + 4 * len(names)) + b"".join(struct.pack("<I", A[n]) for n in names)
        xml = pool + resmap + ns_start + b"".join(body) + ns_end
        return struct.pack("<HHI", 0x0003, 8, 8 + len(xml)) + xml


def manifest():
    a = lambda n, v: ("android", n, v)
    tree = ("manifest", [(None, "package", PACKAGE), a("versionCode", VERSION_CODE), a("versionName", VERSION_NAME)], [
        ("uses-sdk", [a("minSdkVersion", MIN_SDK), a("targetSdkVersion", TARGET_SDK)], []),
        ("uses-permission", [a("name", "android.permission.INTERNET")], []),
        ("uses-permission", [a("name", "android.permission.VIBRATE")], []),
        ("application", [a("theme", THEME), a("label", "Horizon Transit"), a("icon", ICON),
                         a("hardwareAccelerated", True), a("usesCleartextTraffic", True)], [
            ("activity", [a("name", PACKAGE + ".MainActivity"), a("exported", True), a("configChanges", 0xFB0),
                          a("windowSoftInputMode", 0x10)], [
                ("intent-filter", [], [
                    ("action", [a("name", "android.intent.action.MAIN")], []),
                    ("category", [a("name", "android.intent.category.LAUNCHER")], []),
                ]),
            ]),
        ]),
    ])
    return Axml().build(tree)


# ------------------------------------------------------------------ resources.arsc: one drawable, the icon

def resources():
    values = string_pool(["res/drawable/icon.png"])
    types = string_pool(["drawable"])
    keys = string_pool(["icon"])
    spec = struct.pack("<HHIBBHI", 0x0202, 16, 16 + 4, 1, 0, 0, 1) + struct.pack("<I", 0)
    config = struct.pack("<I", 64) + b"\0" * 60
    header = 20 + len(config)
    entry = struct.pack("<HHI", 8, 0, 0) + struct.pack("<HBBI", 8, 0, 0x03, 0)
    typ = struct.pack("<HHIBBHII", 0x0201, header, header + 4 + len(entry), 1, 0, 0, 1, header + 4) + config + struct.pack("<I", 0) + entry
    name = "io.github.flenkii.horizontransit".encode("utf-16-le").ljust(256, b"\0")
    pkg_header = 288
    pkg_body = types + keys + spec + typ
    pkg = struct.pack("<HHII", 0x0200, pkg_header, pkg_header + len(pkg_body), 0x7F) + name + \
        struct.pack("<IIIII", pkg_header, 1, pkg_header + len(types), 1, 0) + pkg_body
    body = values + pkg
    return struct.pack("<HHII", 0x0002, 12, 12 + len(body), 1) + body


# ------------------------------------------------------------------ the icon

def icon_png(size=192):
    """the app's icon: the H on the app's pink-to-orange square (drawn here, so nothing is borrowed)"""
    from PIL import Image, ImageDraw
    big = size * 4
    im = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    grad = Image.new("RGBA", (big, big))
    gp = grad.load()
    for y in range(big):
        for x in range(big):
            t = (x + y) / (2 * big)
            gp[x, y] = (int(255), int(46 + (138 - 46) * t), int(126 + (61 - 126) * t), 255)
    mask = Image.new("L", (big, big), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, big - 1, big - 1), radius=big * 0.23, fill=255)
    im.paste(grad, (0, 0), mask)
    d = ImageDraw.Draw(im)
    s = big / 192.0
    w, gap = 30 * s, 34 * s
    left, right, top, bottom = 52 * s, 140 * s, 44 * s, 148 * s
    d.rounded_rectangle((left, top, left + w, bottom), radius=8 * s, fill="white")
    d.rounded_rectangle((right - w, top, right, bottom), radius=8 * s, fill="white")
    d.rectangle((left + w - 1, 84 * s, right - w + 1, 84 * s + 26 * s), fill="white")
    return im.resize((size, size), Image.LANCZOS)


# ------------------------------------------------------------------ the zip, 4-byte aligned where stored

def write_zip(path, entries):
    """entries: (name, bytes, compress); stored ones start on a 4-byte boundary (like zipalign)"""
    out, central = bytearray(), bytearray()
    for name, data, compress in entries:
        nb = name.encode()
        crc = zlib.crc32(data) & 0xFFFFFFFF
        if compress:
            c = zlib.compressobj(9, zlib.DEFLATED, -15)
            body = c.compress(data) + c.flush()
            method = 8
        else:
            body, method = data, 0
        offset = len(out)
        extra = b""
        if not compress:
            need = (-(offset + 30 + len(nb))) % 4
            extra = b"\0" * need
        out += struct.pack("<IHHHHHIIIHH", 0x04034B50, 20, 0, method, 0, 0x21, crc, len(body), len(data), len(nb), len(extra)) + nb + extra + body
        central += struct.pack("<IHHHHHHIIIHHHHHII", 0x02014B50, 20, 20, 0, method, 0, 0x21, crc, len(body), len(data),
                               len(nb), 0, 0, 0, 0, 0, offset) + nb
    cd = len(out)
    out += central + struct.pack("<IHHHHIIH", 0x06054B50, 0, 0, len(entries), len(entries), len(central), cd, 0)
    with open(path, "wb") as f:
        f.write(out)


def main():
    tools()
    shutil.rmtree(BUILD, ignore_errors=True)
    classes = os.path.join(BUILD, "classes")
    os.makedirs(classes)
    srcs = [os.path.join(dp, f) for dp, _, fs in os.walk(os.path.join(HERE, "src")) for f in fs if f.endswith(".java")]
    run(["javac", "--release", "8", "-nowarn", "-cp", os.path.join(TOOLS, "android-all.jar"), "-d", classes] + srcs)
    dex = os.path.join(BUILD, "classes.dex")
    run(["java", "-cp", os.path.join(TOOLS, "dx.jar"), "com.android.dx.command.Main", "--dex", "--min-sdk-version=%d" % MIN_SDK,
         "--output=" + dex, classes])
    import io
    png = io.BytesIO()
    icon_png().save(png, "PNG", optimize=True)
    html = open(os.path.join(ROOT, "app", "index.html"), "rb").read()
    unsigned = os.path.join(BUILD, "unsigned.apk")
    write_zip(unsigned, [
        ("AndroidManifest.xml", manifest(), True),
        ("classes.dex", open(dex, "rb").read(), True),
        ("resources.arsc", resources(), False),
        ("res/drawable/icon.png", png.getvalue(), False),
        ("assets/index.html", html, True),
    ])
    if not os.path.exists(KEYSTORE):
        run(["keytool", "-genkeypair", "-keystore", KEYSTORE, "-storetype", "PKCS12", "-storepass", KEY_PASS, "-keypass", KEY_PASS,
             "-alias", "horizontransit", "-keyalg", "RSA", "-keysize", "2048", "-validity", "10000",
             "-dname", "CN=Horizon Transit, O=Horizon Transit"])
    signer = os.path.join(BUILD, "signer")
    os.makedirs(signer)
    cp = os.path.join(TOOLS, "apksig.jar")
    run(["javac", "--release", "8", "-nowarn", "-cp", cp, "-d", signer, os.path.join(HERE, "SignApk.java")])
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    run(["java"] + ["--add-exports=java.base/sun.security.%s=ALL-UNNAMED" % m for m in ("x509", "pkcs", "util")] + ["-cp", cp + os.pathsep + signer, "SignApk", unsigned, OUT, KEYSTORE, KEY_PASS, str(MIN_SDK)])
    print("fertig:", OUT, os.path.getsize(OUT), "bytes")


if __name__ == "__main__":
    main()
