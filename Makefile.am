SUBDIRS = src other doc include

messages:
	@set fnord $(MAKEFLAGS); amf=$$2; \
	for subdir in src; do \
	echo "Making messages in $$subdir"; \
	  (cd $$subdir && $(MAKE) messages) \
	   || case "$$amf" in *=*) exit 1;; *k*) fail=yes;; *) exit 1;; esac; \
	done && test -z "$$fail"

