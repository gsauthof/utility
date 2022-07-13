.PHONY: srpm
srpm:
	dnf -y install git-core
	# work around new git safety checks ...
	# we are running as root but Copr checked out this repository as mockbuild:1000 ...
	git config --global --add safe.directory $$PWD/libixxx
	git config --global --add safe.directory $$PWD/libixxxutil
	./build-tar.sh
	rpmbuild --define "_sourcedir $$PWD" --define "_specdir $$PWD" --define "_builddir $$PWD" --define "_rpmdir $(outdir)" --define "_srcrpmdir $(outdir)" -bs gms-utils.spec
