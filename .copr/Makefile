.PHONY: srpm
srpm:
	dnf -y install git-core
	id
	ls -l
	# work around new git safety checks ...
	git config --global --add safe.directory $$PWD/libixxx
	git config --global --add safe.directory $$PWD/libixxxutil
	git submodule update --init
	./build-tar.sh
	rpmbuild --define "_sourcedir $$PWD" --define "_specdir $$PWD" --define "_builddir $$PWD" --define "_rpmdir $(outdir)" --define "_srcrpmdir $(outdir)" -bs gms-utils.spec
