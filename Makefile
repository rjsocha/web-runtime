PROJECTS ?= dotnet dotnet-aot go java java-javalin cpp rust

.PHONY: all clean $(PROJECTS)

all: $(PROJECTS)

$(PROJECTS):
	$(MAKE) -C $@

clean:
	for p in $(PROJECTS); do $(MAKE) -C $$p clean; done
