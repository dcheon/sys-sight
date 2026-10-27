FROM ubuntu:24.04
RUN apt-get update && apt-get install -y build-essential cmake ca-certificates && \
rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .

RUN cmake -S . -B build -DSYS_SIGHT_BUILD_TESTS=ON && cmake --build build --parallel
CMD ["ctest", "--test-dir", "build", "--output-on-failure", "--no-tests=error"]