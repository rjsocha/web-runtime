IMAGE   ?= wyga/web-runtime-java-javalin
PAYLOAD ?= $(IMAGE)-payload:latest
RUNTIME_LINUX   ?= eclipse-temurin:25-jre
RUNTIME_WINDOWS ?= eclipse-temurin:25-jre-nanoserver-ltsc2022
WINDOWS  ?= 2022:ltsc2022 2025:ltsc2025
LATEST   ?= 2025
PLATFORMS     ?= linux/amd64,linux/arm64,windows/amd64
APP_PLATFORMS ?= linux/amd64,linux/arm64,windows/amd64
BUILDER  ?= java-javalin-builder
RUN_TAG  ?= latest
PORT     ?= 8080

OUT = --provenance=false --sbom=false --output type=image,oci-mediatypes=false,push=true

.PHONY: all payload linux windows multi builder run

all: payload multi

payload: builder
	docker buildx build --builder $(BUILDER) --platform $(APP_PLATFORMS) -f Dockerfile.payload -t $(PAYLOAD) $(OUT) app

linux: builder
	docker buildx build --builder $(BUILDER) --platform linux/amd64 -f Dockerfile.runtime --build-arg PAYLOAD=$(PAYLOAD) --build-arg RUNTIME=$(RUNTIME_LINUX) -t $(IMAGE):linux $(OUT) .

windows: builder
	docker buildx build --builder $(BUILDER) --platform windows/amd64 -f Dockerfile.runtime --build-arg PAYLOAD=$(PAYLOAD) --build-arg RUNTIME=$(RUNTIME_WINDOWS) -t $(IMAGE):windows $(OUT) .

multi: builder
	for v in $(WINDOWS); do \
	  tag=$${v%%:*}; \
	  if [ "$$tag" = "$(LATEST)" ]; then latest="-t $(IMAGE):latest"; else latest=""; fi; \
	  docker buildx build --builder $(BUILDER) --platform $(PLATFORMS) \
	    -f Dockerfile.multi \
	    --build-arg PAYLOAD=$(PAYLOAD) \
	    --build-arg RUNTIME_LINUX=$(RUNTIME_LINUX) \
	    --build-arg RUNTIME_WINDOWS=eclipse-temurin:25-jre-nanoserver-$${v##*:} \
	    -t $(IMAGE):$$tag $$latest $(OUT) . ; \
	done

builder:
	docker buildx inspect $(BUILDER) >/dev/null 2>&1 || docker buildx create --name $(BUILDER) --driver docker-container --bootstrap >/dev/null

run:
	docker run --rm -p $(PORT):8080 $(IMAGE):$(RUN_TAG)
