#include <stdio.h>


struct seq_str {
	unsigned int bit[3];	//Data produced by the generators
	//to do... 
	// Inserire codice per la mutua esclusione
};
static struct seq_str seq_data;

static inline int b2d(struct seq_str* seq) {
	return seq->bit[0] + seq->bit[1] * 2 + seq->bit[2] * 4;
}

void main() {
	seq_data.bit[0] = 0;
    seq_data.bit[1] = 1;
    seq_data.bit[2] = 0;
    
    int d = b2d(&seq_data);
    printf("Val = %i\n", d);
}

