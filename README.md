# autolink

Autolink base protocol communication framework.

## Docker Quick Start

Docker related scripts are in the `docker/` directory.

### Image naming

The project now uses the following image names:

- `autolink.x86_64`
- `autolink.x86_64.nvidia`
- `autolink.aarch64`

### Run container

Use `docker/run.py` to auto-detect platform, select image, and run a container.

```bash
python3 docker/run.py
```

Examples:

```bash
# x86_64 standard image
python3 docker/run.py -p x86_64 -n no

# x86_64 nvidia image
python3 docker/run.py -p x86_64 -n yes

# aarch64 image
python3 docker/run.py -p aarch64

# pass extra docker run args
python3 docker/run.py -- --rm
```

### Build image

Build scripts:

- `docker/build_docker_x86_64.py`
- `docker/build_docker_aarch64.py`

Examples:

```bash
# build x86_64 standard
python3 docker/build_docker_x86_64.py -f dockerfile/autolink.x86_64.dockerfile

# build x86_64 nvidia
python3 docker/build_docker_x86_64.py -n

# build aarch64
python3 docker/build_docker_aarch64.py -f dockerfile/autolink.aarch64.dockerfile
```
