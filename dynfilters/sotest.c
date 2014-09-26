#include <dlfcn.h>
#include <stdio.h>



int main(void)
{
    int (*filter)(FILE *in, FILE *out);
    void *handle;
    char *error;

    if ((handle = dlopen("./htuml2txt.so", 
			 RTLD_NOW)) == NULL) {
	fprintf(stderr, "sotest: %s\n", dlerror());
	return 1;
    }
    
    filter = dlsym(handle, "filter_func");
    if ((error = dlerror()) != NULL) {
	fprintf(stderr, "sotest: %s\n", error);
	return 1;
    }

    (*filter)(stdin, stdout);
    
    dlclose(handle);
    return 0;
}
