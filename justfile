# Build and test mosh-tcp

build:
    nix-build default.nix

# Run autotools configure + make (for development)
dev-build:
    ./autogen.sh
    ./configure --enable-compile-warnings=error
    make -j$(nproc)

test:
    make check

clean:
    make clean || true

# Quick smoke test: verify server starts with TCP protocol
smoke-test: build
    result/bin/mosh-server new -P tcp -p 0 -- /bin/true 2>&1 | head -5
