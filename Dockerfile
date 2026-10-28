# Build and test RealityCheck in a clean Linux environment (no third-party dependencies, no network needed at run time).
#   docker build -t realitycheck .      docker run --rm realitycheck            (runs the tests)
#   docker run --rm realitycheck ./build/rc doctor
FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends build-essential cmake curl ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /rc
COPY . .
RUN cmake -S . -B build && cmake --build build -j 4
CMD ["./build/rc_tests"]
