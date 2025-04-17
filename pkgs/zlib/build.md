# zlib

Compression library implementing the deflate compression method found in gzip and PKZIP

- PKG_HOMEPAGE=https://www.zlib.net/
- PKG_LICENSE="ZLIB"

| key          | value                      |
| ------------ | -------------------------- |
| PKG_VERSION  | 1.3.1                      |
| PKG_BASENAME | zlib-{PKG_VERSION}         |
| PKG_EXTNAME  | .tar.gz                    |
| PKG_SRCURL   | https://zlib.net/fossils/x |

| Depends |
| ------- |
| zlib    |

## configure

```sh
echo PKG_VERSION=${PKG_SRCURL}
./configure --static --archs="-fPIC" --prefix="${OUTPUT_DIR}"
```

## build

```sh
make -j"${JOBS}" install
```

## check

```sh
test -f "${OUTPUT_DIR}/lib/libz.a"
```
