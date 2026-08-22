IMAGE   ?= wyga/web-runtime-cpp
WINDOWS  ?= 2022:ltsc2022 2025:ltsc2025
LATEST   ?= 2025
PLATFORMS ?= linux/amd64,linux/arm64,windows/amd64
BUILDER  ?= cpp-builder
RUN_TAG  ?= latest
PORT     ?= 8080

OUT = --provenance=false --sbom=false --output type=image,oci-mediatypes=false,push=true

.PHONY: all image check builder run

all: image

check: builder
	docker buildx build --builder $(BUILDER) --platform $(PLATFORMS) --target build --progress plain --output type=cacheonly -f Dockerfile app

image: builder
	for v in $(WINDOWS); do \
	  tag=$${v%%:*}; \
	  if [ "$$tag" = "$(LATEST)" ]; then latest="-t $(IMAGE):latest"; else latest=""; fi; \
	  docker buildx build --builder $(BUILDER) --platform $(PLATFORMS) -f Dockerfile \
	    --build-arg BASE_WINDOWS=mcr.microsoft.com/windows/nanoserver:$${v##*:} \
	    -t $(IMAGE):$$tag $$latest $(OUT) app ; \
	done

builder:
	docker buildx inspect $(BUILDER) >/dev/null 2>&1 || docker buildx create --name $(BUILDER) --driver docker-container --bootstrap >/dev/null

run:
	docker run --rm -p $(PORT):8080 $(IMAGE):$(RUN_TAG)
