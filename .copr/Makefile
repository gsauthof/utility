.PHONY: srpm
srpm:
	./build-tar.sh
	rpmbuild --define "_sourcedir $(outdir)" --define "_specdir $(outdir)" --define "_rpmdir out" --define "_srcrpmdir out" --define "_builddir $(outdir)" -bs gms-utils.spec
