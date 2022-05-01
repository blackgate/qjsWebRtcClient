# QuickJS WebRTC client

A QuickJS WebRTC client implementation using [libdatachannel](https://github.com/paullouisageneau/libdatachannel)

## Installing dependencies

Before running the install script make sure you have the following packages installed:

```sh
sudo apt install pkg-config libssl-dev libsrtp2-dev libusrsctp-dev
```

Run the install script (this will install libdatachannel)

```sh
./make-install-deps.sh
```

## Building

```sh
mkdir build
cd build
cmake ..
make
```

## Run examples

```
cd examples/<example>
python3 -m http.server & qjs -i -e "import('backend.js')"
```

Then open `http://localhost:8000` on the browser. 