/*
 * Minimal LLEXT sample extension.
 * Build with: west build -t hello_world_ext
 */
extern void printk(const char *fmt, ...);
 
void hello_world(void)
{
	printk("hello from LLEXT\n");
}
 
void hello_board(void)
{
	printk("board extension call is working\n");
}