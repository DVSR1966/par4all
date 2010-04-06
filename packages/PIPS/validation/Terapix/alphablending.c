#define n 512
#define alpha 40
void alphablending(short src0[n][n], short src1[n][n], short result[n][n])
{
    unsigned int i,j;
    for(i=0;i<n;i++)
        for(j=0;j<n;j++)
            result[i][j]=(
                    alpha*src0[i][j]
                    +
                    (100-alpha)*src1[i][j]
                    )/100;
}

caller()
{
    short a[n][n],b[n][n],c[n][n];
    alphablending(a,b,c);
}
