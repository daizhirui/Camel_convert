all:
	make -f Makefile.Darwin
	make -f Makefile.Win32
	make -f Makefile.Win64
	make -f Makefile.Linux64
	make -f Makefile.Linux32
	cd ..;tar -cjf convert-v3-elfreader-$(shell date "+%Y-%m-%d").tar.gz release;cd -
