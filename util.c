/*********** util.c file ****************/
#include "util.h"

extern PROC *running;
extern MINODE *root, minode[NMINODE];
extern char gpath[128];
extern char *name[32];
extern int n, inode_start, dev, imap, bmap, ninodes, nblocks;
int r;

int get_block(int dev, int blk, char *buf)
{
    lseek(dev, (long)blk*BLKSIZE, 0);
    r = read(dev, buf, BLKSIZE);

    return r;
}   

int put_block(int dev, int blk, char *buf)
{
    lseek(dev, (long)blk*BLKSIZE, 0);
    r = write(dev, buf, BLKSIZE);

    return r;
}   

int tokenize(char *pathname)
{
    int i;
    char *s;
    printf("tokenize %s\n", pathname);

    strcpy(gpath, pathname);   // tokens are in global gpath[ ]
    n = 0;

    s = strtok(gpath, "/");
    while(s){
        name[n] = s;
        n++;
        s = strtok(0, "/");
    }

    for (i= 0; i<n; i++)
        printf("%s  ", name[i]);
    printf("\n");

    return 0;
}
    
// return minode pointer to loaded INODE
MINODE *iget(int dev, int ino)
{
    int i;
    MINODE *mip;
    char buf[BLKSIZE];
    int blk, offset;
    INODE *ip;

    for (i=0; i<NMINODE; i++){
        mip = &minode[i];
        if (mip->dev == dev && mip->ino == ino){
            mip->refCount++;
            //printf("found [%d %d] as minode[%d] in core\n", dev, ino, i);
            return mip;
        }
    }
    
    for (i=0; i<NMINODE; i++){
        mip = &minode[i];
        if (mip->refCount == 0){
            //printf("allocating NEW minode[%d] for [%d %d]\n", i, dev, ino);
            mip->refCount = 1;
            mip->dev = dev;
            mip->ino = ino;

            // get INODE of ino into buf[ ]    
            blk    = (ino-1)/8 + inode_start;
            offset = (ino-1) % 8;

            //printf("iget: ino=%d blk=%d offset=%d\n", ino, blk, offset);

            get_block(dev, blk, buf);
            ip = (INODE *)buf + offset;
            // copy INODE to mp->INODE
            mip->inode = *ip;
            return mip;
        }
    }   
    printf("PANIC: no more free minodes\n");
    return 0;
}
    
void iput(MINODE *mip)
{
    int block, offset;
    char buf[BLKSIZE];
    INODE *ip;

    if (mip==0) return;
    mip->refCount--;
    if (mip->refCount > 0)  // minode is still in use
        return;
    if (!mip->dirty)        // INODE has not changed; no need to write back
        return;

    /* write INODE back to disk */
    /***** NOTE *******************************************
        For mountroot, we never MODIFY any loaded INODE
        so no need to write it back
        FOR LATER WROK: MUST write INODE back to disk if refCount==0 && DIRTY
        Write YOUR code here to write INODE back to disk
    ********************************************************/

    printf(" iput : inode_start=%d\n", inode_start);

    block = (mip->ino - 1) / 8 + inode_start;
    offset = (mip->ino -1) % 8;

    printf(" iput : block=%d, offset=%d\n", block, offset);

    get_block(mip->dev, block, buf);
    ip = (INODE *)buf + offset;
    *ip = mip->inode;
    put_block(mip->dev, block, buf);
    mip->refCount = 0;

} 

int search(MINODE *mip, char *name)
{
    char *cp, sbuf[BLKSIZE], temp[256];
    DIR *dp;
    INODE *ip;

    printf("search for %s in MINODE = [%d, %d]\n", name, mip->dev, mip->ino);
    ip = &(mip->inode);

    /*** search for name in mip's data blocks: ASSUME i_block[0] ONLY ***/

    get_block(dev, ip->i_block[0], sbuf);
    dp = (DIR *)sbuf;
    cp = sbuf;
    printf("  ino   rlen  nlen  name\n");

    while (cp < sbuf + BLKSIZE){
        strncpy(temp, dp->name, dp->name_len);
        temp[dp->name_len] = 0;
        printf("%4d  %4d  %4d    %s\n", 
            dp->inode, dp->rec_len, dp->name_len, temp);
        if (strcmp(temp, name)==0){
            printf("found %s : ino = %d\n", temp, dp->inode);
            return dp->inode;
        }
        cp += dp->rec_len;
        dp = (DIR *)cp;
    }
    return 0;
}
    
int getino(char *pathname)
{
    int i, ino;// blk, disp;
    //char buf[BLKSIZE];
    //INODE *ip;
    MINODE *mip;

    printf("getino: pathname=%s\n", pathname);
    if (strcmp(pathname, "/")==0)
        return 2;

    // starting mip = root OR CWD
    if (pathname[0]=='/')
        mip = root;
    else
        mip = running->cwd;

    mip->refCount++;         // because we iput(mip) later

    tokenize(pathname);
    for (i=0; i<n; i++){
        printf("===========================================\n");
        printf("getino: i=%d name[%d]=%s\n", i, i, name[i]);
 
        ino = search(mip, name[i]);

        if (ino==0){
            iput(mip);
            printf("name %s does not exist\n", name[i]);
            return 0;
        }
        iput(mip);                // release current mip
        mip = iget(dev, ino);     // get next mip
    }

    iput(mip);                   // release mip  
    return ino;
}

int enter_name(MINODE *pip, int myino, char *myname){
    char buf[BLKSIZE], *cp, temp[256];
    DIR *dp;
    int block_i, i, ideal_len, need_len, remain, blk;
    //printf(" enter_name : myname=%s\t ideal_len=%d\t iblocks=%d\n", myname, ideal_len, pip->inode.i_blocks);
    //
    printf(" enter_name : search(pip)\n");
    search(pip, "b");

    //u32 *ino=malloc(8);
    //int p_ino = findino(pip, ino);
    //pip = iget(dev, p_ino);

    printf(" enter_name : search(pip)2\n");
    search(pip, "b");

    need_len = 4 * ((8 + (strlen(myname)) + 3) / 4);
    printf("need len for %s is %d\n", myname, need_len);

    for (i=0; i<12; i++){ // find empty block
        if (pip->inode.i_block[i]==0) break;
        get_block(pip->dev, pip->inode.i_block[i], buf); // get that empty block
        printf("get_block : i=%d\n", i);
        block_i = i;
        dp = (DIR *)buf;
        cp = buf;

        blk = pip->inode.i_block[i];

        printf(" stepping through parent data block[i] = %d\n", blk);
        while (cp + dp->rec_len < buf + BLKSIZE){
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0;

            printf("[%d %s] ", dp->rec_len, temp);
            
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
        printf("[%d %s]\n", dp->rec_len, dp->name);

        ideal_len = 4 * ((8 + dp->name_len + 3) / 4);

        printf("ideal_len=%d\n", ideal_len);
        remain = dp->rec_len - ideal_len;
        printf("remain=%d\n", remain);

        if (remain >= need_len){
            dp->rec_len = ideal_len; // trim last rec_len to ideal_len

            cp += dp->rec_len;
            dp = (DIR*)cp;
            dp->inode = myino;
            dp->rec_len = remain;
            dp->name_len = strlen(myname);
            strcpy(dp->name, myname);
        }
    }

    //if (pip->inode.i_block[i] == 0){ // (5) no space in existing data blocks
    //    printf(" [X] inode has ran out of room! Allocating new one.\n");
    //    bzero(buf, BLKSIZE); // buf will be our new block
    //    dp = (DIR *)buf;
    //    dp->inode = myino;
    //    dp->rec_len = BLKSIZE;
    //    dp->name_len = strlen(myname);
    //    strcpy(dp->name, myname);
    //    pip->inode.i_size += BLKSIZE; // increment parent size by BLKSIZE
    //}

    printf("put_block : i=%d\n", block_i);
    put_block(pip->dev, pip->inode.i_block[block_i], buf);
    printf("write parent data block=%d to disk\n", blk);


    return 0;
}
    
int findmyname(MINODE *parent, u32 myino, char *myname) 
{
    DIR *dp;
    char buf[BLKSIZE], temp[256], *cp;

    get_block(dev, parent->inode.i_block[0], buf);
    dp = (DIR *)buf;
    cp = buf;

    while (cp < buf + BLKSIZE){
        strncpy(temp, dp->name, dp->name_len);
        temp[dp->name_len] = 0;

        if (dp->inode == myino)
            strcpy(myname, temp);

        cp += dp->rec_len;
        dp = (DIR *)cp;
    }

    return 0;
}

int findino(MINODE *mip, u32 *myino) // myino = ino of . return ino of ..
{
    char buf[BLKSIZE], *cp;   
    DIR *dp;

    get_block(mip->dev, mip->inode.i_block[0], buf);
    cp = buf; 
    dp = (DIR *)buf;
    *myino = dp->inode;
    cp += dp->rec_len;
    dp = (DIR *)cp;
    return dp->inode;
}

int abs_path(char *path){
    if (path[0] == '/')
        return 0;
    else
        return -1;
}

int tst_bit(char *buf, int bit){
    int i = bit / 8, j = bit % 8;

    if (buf[i] & (1 << j))
        return 1;

    return 0;
}

int set_bit(char *buf, int bit){
    int i = bit / 8, j = bit % 8;

    buf[i] |= (1 << j);

    return 0;
}

int clr_bit(char *buf, int bit){
    int i = bit / 8, j = bit % 8;

    buf[i] &= ~(1 << j);

    return 0;
}

int dec_free_inodes(int dev){
    char buf[BLKSIZE];
    get_block(dev, 1, buf); // dec the super table
    sp = (SUPER *)buf;
    sp->s_free_inodes_count--;
    put_block(dev, 1, buf);
    get_block(dev, 2, buf);
    gp = (GD *)buf; // dec the GD table
    gp->bg_free_inodes_count--;
    put_block(dev, 2, buf);
    return 0;
}

int dec_free_blocks(int dev){
    char buf[BLKSIZE];
    get_block(dev, 1, buf); // dec the super table
    sp = (SUPER *)buf;
    sp->s_free_blocks_count--;
    put_block(dev, 1, buf);
    get_block(dev, 2, buf);
    gp = (GD *)buf; // dec the GD table
    gp->bg_free_blocks_count--;
    put_block(dev, 2, buf);
    return 0;
}

int ialloc(int dev){
    int i;
    char buf[BLKSIZE];

    get_block(dev, imap, buf);

    for (i=0; i < ninodes; i++){
        if (tst_bit(buf, i) == 0){
            set_bit(buf, i);
            put_block(dev, imap, buf);
            dec_free_inodes(dev);
            printf("allocated ino = %d\n", i+1); // bits 0..n, ino 1..n+1
            return i+1;
        }
    }
    return 0;
}

int balloc(int dev){
    int i;
    char buf[BLKSIZE];

    get_block(dev, bmap, buf);

    for (i=0; i < nblocks; i++){
        if (tst_bit(buf, i) == 0){
            set_bit(buf, i);
            put_block(dev, bmap, buf);
            printf("allocated block = %d\n", i+1); // bits 0..n, ino 1..n+1
            dec_free_blocks(dev);
            return i+1;
        }
    }
    return 0;
}
