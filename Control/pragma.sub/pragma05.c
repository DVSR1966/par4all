int main() {
  int i;
  
#pragma toto
  { 
    int i;
    i=0;
    i++;
  }
#pragma X
  i=0;
  
  return i;
}
