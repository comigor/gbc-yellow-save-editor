# syntax=docker/dockerfile:1
FROM debian:12-slim AS toolchain
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates curl make python3 clang libc6-dev \
    && rm -rf /var/lib/apt/lists/*
ARG TARGETARCH
RUN case "$TARGETARCH" in \
      amd64) archive=gbdk-linux64.tar.gz; sha=d7857a5f6d135ee4c249043ca26aad9f2ec8ab5d4106d97720d404114f42605c ;; \
      arm64) archive=gbdk-linux-arm64.tar.gz; sha=31eb2235f0fdb60163d0b1e9574a022098d6069cd56606a1daca4478a46e0439 ;; \
      *) exit 1 ;; \
    esac \
    && curl -fL --retry 3 "https://github.com/gbdk-2020/gbdk-2020/releases/download/4.5.0/$archive" -o /tmp/gbdk.tar.gz \
    && printf '%s  /tmp/gbdk.tar.gz\n' "$sha" | sha256sum -c - \
    && tar -xzf /tmp/gbdk.tar.gz -C /opt \
    && rm /tmp/gbdk.tar.gz
ENV GBDK_HOME=/opt/gbdk
WORKDIR /src

FROM toolchain AS build
COPY . .
RUN make -j2 && make check

FROM toolchain AS private-build
COPY . .
RUN --mount=type=secret,id=yellow_rom,required=true \
    make -j2 YELLOW_ROM=/run/secrets/yellow_rom && make check

FROM scratch AS private-artifact
COPY --from=private-build /src/build/private/yellow-editor-sprites.gbc /yellow-editor-sprites.gbc

FROM scratch AS artifact
COPY --from=build /src/build/yellow-editor.gbc /yellow-editor.gbc
