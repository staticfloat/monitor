//
//  monkext.c
//  monkext
//
//  Created by Elliot Saba on 7/13/14.
//
//

#include <mach/mach_types.h>
#include <mach-o/loader.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/syscall.h>

#ifdef DEBUG
#define DLOG(args...)   printf(args)
#elif
#define DLOG(args...)   /* */
#endif

#define KERNEL_MH_START_ADDR       0xffffff8000200000

/* Borrowed from kernel source. It doesn't exist in Kernel.framework. */
struct nlist_64 {
    union {
        uint32_t  n_strx;   /* index into the string table */
    } n_un;
    uint8_t n_type;         /* type flag, see below */
    uint8_t n_sect;         /* section number or NO_SECT */
    uint16_t n_desc;        /* see <mach-o/stab.h> */
    uint64_t n_value;       /* value of this symbol (or stab offset) */
};

typedef	int32_t	sy_call_t(struct proc *, void *, int *);
typedef	void	sy_munge_t(const void *, void *);

struct sysent {		/* system call table */
	int16_t		sy_narg;	/* number of args */
	int8_t		sy_resv;	/* reserved  */
	int8_t		sy_flags;	/* flags */
	sy_call_t	*sy_call;	/* implementing function */
	sy_munge_t	*sy_arg_munge32; /* system call arguments munger for 32-bit process */
	sy_munge_t	*sy_arg_munge64; /* system call arguments munger for 64-bit process */
	int32_t		sy_return_type; /* system call return types */
	uint16_t	sy_arg_bytes;	/* Total size of arguments in bytes for
                                 * 32-bit system calls
                                 */
};

kern_return_t monkext_start(kmod_info_t * ki, void *d);
kern_return_t monkext_stop(kmod_info_t *ki, void *d);


struct segment_command_64 *
find_segment_64(struct mach_header_64 *mh, const char *segname)
{
    struct load_command *lc;
    struct segment_command_64 *seg, *foundseg = NULL;
    
    /* First LC begins straight after the mach header */
    lc = (struct load_command *)((uint64_t)mh + sizeof(struct mach_header_64));
    while ((uint64_t)lc < (uint64_t)mh + (uint64_t)mh->sizeofcmds) {
        if (lc->cmd == LC_SEGMENT_64) {
            /* Check load command's segment name */
            seg = (struct segment_command_64 *)lc;
            if (strcmp(seg->segname, segname) == 0) {
                foundseg = seg;
                break;
            }
        }
        
        /* Next LC */
        lc = (struct load_command *)((uint64_t)lc + (uint64_t)lc->cmdsize);
    }
    
    /* Return the segment (NULL if we didn't find it) */
    return foundseg;
}

struct section_64 *
find_section_64(struct segment_command_64 *seg, const char *name)
{
    struct section_64 *sect, *foundsect = NULL;
    u_int i = 0;
    
    /* First section begins straight after the segment header */
    for (i = 0, sect = (struct section_64 *)((uint64_t)seg + (uint64_t)sizeof(struct segment_command_64));
         i < seg->nsects;
         i++, sect = (struct section_64 *)((uint64_t)sect + sizeof(struct section_64)))
    {
        /* Check section name */
        if (strcmp(sect->sectname, name) == 0) {
            foundsect = sect;
            break;
        }
    }
    
    /* Return the section (NULL if we didn't find it) */
    return foundsect;
}

struct load_command *
find_load_command(struct mach_header_64 *mh, uint32_t cmd)
{
    struct load_command *lc, *foundlc;
    
    /* First LC begins straight after the mach header */
    lc = (struct load_command *)((uint64_t)mh + sizeof(struct mach_header_64));
    while ((uint64_t)lc < (uint64_t)mh + (uint64_t)mh->sizeofcmds) {
        if (lc->cmd == cmd) {
            foundlc = (struct load_command *)lc;
            break;
        }
        
        /* Next LC */
        lc = (struct load_command *)((uint64_t)lc + (uint64_t)lc->cmdsize);
    }
    
    /* Return the load command (NULL if we didn't find it) */
    return foundlc;
}

void *
find_symbol(struct mach_header_64 *mh, const char *name)
{
    struct symtab_command *symtab = NULL;
    struct segment_command_64 *linkedit = NULL;
    void *strtab = NULL;
    struct nlist_64 *nl = NULL;
    char *str;
    uint64_t i;
    void *addr = NULL;
    
    /*
     * Check header
     */
    if (mh->magic != MH_MAGIC_64) {
        DLOG("FAIL: magic number doesn't match - 0x%x\n", mh->magic);
        return NULL;
    }
    
    /*
     * Find the LINKEDIT and SYMTAB sections
     */
    linkedit = find_segment_64(mh, SEG_LINKEDIT);
    if (!linkedit) {
        DLOG("FAIL: couldn't find __LINKEDIT\n");
        return NULL;
    }
    
    symtab = (struct symtab_command *)find_load_command(mh, LC_SYMTAB);
    if (!symtab) {
        DLOG("FAIL: couldn't find SYMTAB\n");
        return NULL;
    }
    
    /*
     * Enumerate symbols until we find the one we're after
     */
    strtab = (void *)((int64_t)linkedit->vmaddr + (symtab->stroff - symtab->symoff));
    for (i = 0, nl = (struct nlist_64 *)linkedit->vmaddr;
         i < symtab->nsyms;
         i++, nl = (struct nlist_64 *)((uint64_t)nl + sizeof(struct nlist_64)))
    {
        str = (char *)strtab + nl->n_un.n_strx;
        if (strcmp(str, name) == 0) {
            addr = (void *)nl->n_value;
        }
    }
    
    /* Return the address (NULL if we didn't find it) */
    return addr;
}

// Find the sysent
static struct sysent * find_sysent () {
    struct sysent *table;
    int *nsysent = (int *)find_symbol( (struct mach_header_64 *)KERNEL_MH_START_ADDR, "_nsysent");
    table = (struct sysent *)(((uint64_t)nsysent) -
                              ((uint64_t)sizeof(struct sysent) * (uint64_t)*nsysent));
    
    if (table[SYS_syscall].sy_narg == 0 &&
        table[SYS_exit].sy_narg == 1 &&
        table[SYS_fork].sy_narg == 0 &&
        table[SYS_read].sy_narg == 3 &&
        table[SYS_wait4].sy_narg == 4 &&
        table[SYS_ptrace].sy_narg == 4)
    {
        return table;
    } else {
        return NULL;
    }
}

kern_return_t monkext_start(kmod_info_t * ki, void *d)
{
    struct sysent * s = find_sysent();
    DLOG("YOLO");
   
    return KERN_SUCCESS;
}

kern_return_t monkext_stop(kmod_info_t *ki, void *d)
{
    return KERN_SUCCESS;
}
