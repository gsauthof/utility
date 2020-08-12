.PHONY: srpm
srpm:
	dnf -y install git-core
	git submodule update --init
	./build-tar.sh
	rpmbuild --define "_sourcedir $$PWD" --define "_specdir $$PWD" --define "_builddir $$PWD" --define "_rpmdir $(outdir)" --define "_srcrpmdir $(outdir)" -bs gms-utils.spec
