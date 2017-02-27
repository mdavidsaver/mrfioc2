
#include <linux/mm.h>

#include "mrf.h"

static
int mrfev_open(struct inode *inode, struct file *fp)
{
    int ret;
    struct mrf_priv *priv = container_of(inode->i_cdev, struct mrf_priv, cdev);
    struct mrf_file *mfile;

    mfile = kzalloc(sizeof(*mfile), GFP_KERNEL);
    if(!mfile) return -ENOMEM;
    mfile->priv = priv;

    if(!(ret=try_module_get(THIS_MODULE))) {
        kzfree(mfile);
        return ret;
    }

    //TODO cdev_get

    fp->private_data = mfile;

    return 0;
}

static
int mrfev_close(struct inode *inode, struct file *fp)
{
    struct mrf_file *mfile = fp->private_data;

    fp->private_data = NULL; // paranoia

    module_put(THIS_MODULE);

    kzfree(mfile);
    return 0;
}

static
ssize_t mrfev_read (struct file *, char __user *, size_t, loff_t *)
{
    return -EINVAL;
}

static
ssize_t mrfev_write (struct file *, const char __user *, size_t, loff_t *)
{
    return -EINVAL;
}

static
long mrfev_ioctl (struct file *fp, unsigned int cmd, unsigned long arg)
{
    struct mrf_file *mfile = fp->private_data;
    struct mrf_priv *priv = mfile->priv;
    long ret;
    void *parg = (void*)arg;
    union {
        u32 ival;
    } uval;

    if(_IOC_SIZE(cmd)>sizeof(uval)) return -EINVAL;

    if(_IOC_DIR(cmd)&_IOC_WRITE) {
        long ret = copy_from_user(&uval, parg, _IOC_SIZE(cmd));
        if(ret) return ret;
    }

    switch(cmd) {
    case MRFEV_GET_VERSION:
        uval.ival = MRFEV_GET_VERSION_CURRENT;
        ret = 0;
        break;
    case MRFEV_SET_INTERESTED:
        spin_lock_irq(priv->file_wait.lock);
        mfile->interested = uval.ival;
        if(priv->irq_active&mfile->interested&~mfile->last) {
            wake_up_all_locked(priv->file_wait);
        }
        spin_unlock_irq(priv->file_wait.lock);
        ret = 0;
        break;
    case MRFEV_GET_INTERESTED:
        spin_lock_irq(priv->file_wait.lock);
        uval.ival = mfile->interested;
        spin_unlock_irq(priv->file_wait.lock);
        ret = 0;
        break;
    case MRFEV_GET_ACTIVE:
    {
        spin_lock_irq(priv->file_wait.lock);
        if(!(fp->f_flags&O_NONBLOCK)) {
            ret = wait_event_interruptible_locked_irq(priv->file_wait,
                                                      priv->irq_active&mfile->interested&~mfile->last!=0
                                                      || mfile->abort);
            if(!ret && mfile->abort) ret = -ECANCELED;
            mfile->abort = 0;
        } else {
            ret = 0;
        }
        uval.ival = priv->irq_active&mfile->interested;
        spin_unlock_irq(priv->file_wait.lock);
    }
        break;
    case MRFEV_ABORT:
        spin_lock_irq(priv->file_wait.lock);
        mfile->abort = 1;
        wake_up_all_locked(priv->file_wait);
        spin_unlock_irq(priv->file_wait.lock);
        ret = 0;
        break;
    default:
        ret = -EINVAL;
    }

    if(_IOC_DIR(cmd)&_IOC_READ) {
        long ret = copy_to_user(parg, &uval, _IOC_SIZE(cmd));
        if(ret) return ret;
    }

    return ret;
}

static
int mrfev_mmap (struct file *fp, struct vm_area_struct *vma)
{
    struct mrf_file *mfile = fp->private_data;
    struct mrf_priv *priv = mfile->priv;
    resource_size_t rsize = pci_resource_len(priv->pdev, 0);

    if(vma->vm_end < vma->vm_start)
        return -EINVAL;

    if (vma->vm_end - vma->vm_start > PAGE_ALIGN(rsize)) {
        dev_err(&dev->dev, "mmap alignment/size test fails %lx %lx %u\n",
                vma->vm_start, vma->vm_end, (unsigned)PAGE_ALIGN(rsize));
        return -EINVAL;
    }

    vma->vm_flags |= VM_IO | VM_RESERVED;
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

    return remap_pfn_range(vma,
                           vma->vm_start,
                           pci_resource_start(priv->pdev, 0) >> PAGE_SHIFT,
                           vma->vm_end - vma->vm_start,
                           vma->vm_page_prot);
}

struct file_operations irqdev_ops =
{
    .owner = THIS_MODULE,
    .read  = mrfev_read,
    .write = mrfev_write,
    .unlocked_ioctl = mrfev_ioctl,
    .mmap  = mrfev_mmap,
    .open  = mrfev_open,
    .release = mrfev_close
};
