/* SPDX-License-Identifier: MIT -- validates the REAL Windows descriptors/queries,
 * NOT the implementation of the providers. No acquisition is attempted.
 */
#include "internal.h"
int main(int argc,char **argv) {
    size_t n,i;const inv_metric_desc *const *metrics=inv_registry(&n);
    inv_plan *p=(inv_plan *)malloc(sizeof(*p));char error[512],query[65537];
    if(!p)return 1;
    if(inv_registry_validate(error,sizeof(error))!=INV_OK){fprintf(stderr,"%s\n",error);free(p);return 1;}
    for(i=0;i<n;i++) {
#if INV_EMBED_DOCS
        if(!inv_metric_documentation(metrics[i]->key)){fprintf(stderr,"Missing doc: %s\n",metrics[i]->key);free(p);return 1;}
#else
        (void)metrics;
#endif
    }
    for(i=1;i<(size_t)argc;i++) {
        size_t bytes;FILE *f=fopen(argv[i],"rb");
        if(!f){perror(argv[i]);free(p);return 1;}
        bytes=fread(query,1,sizeof(query)-1,f);
        if(ferror(f)||!feof(f)){fclose(f);free(p);return 1;}
        fclose(f);query[bytes]=0;
        if(inv_query_parse(query,p,error,sizeof(error))!=INV_OK||inv_plan_validate(p,error,sizeof(error))!=INV_OK) {
            fprintf(stderr,"%s: %s\n",argv[i],error);free(p);return 1;
        }
    }
    free(p);printf("Windows schema only: %zu metric descriptors; %d query files validated (no WinAPI execution)\n",n,argc-1);return 0;
}
