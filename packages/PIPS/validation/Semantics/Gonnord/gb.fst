//Generated by c2fsm -s 
model gb {
var bchange,bleak,l,t,x;
//@parameters bchange;
states start,lbl_16,lbl_7,lbl_8,lbl_9;
transition t_46 :={
  from  := start;
  to    := lbl_7;
  guard := true;
  action:= bleak' = 1, l' = 0, t' = 0, x' = 0;
};
transition t_40 :={
  from  := lbl_16;
  to    := lbl_7;
  guard := (50 <= x);
  action:= bleak' = 1, x' = 0;
};
transition t_42 :={
  from  := lbl_16;
  to    := lbl_7;
  guard := true;
  action:= t' = t+1, x' = x+1;
};
transition t_27 :={
  from  := lbl_7;
  to    := lbl_8;
  guard := true;
  action:=;
};
transition t_28 :={
  from  := lbl_8;
  to    := lbl_9;
  guard := (1 = bleak);
  action:=;
};
transition t_29 :={
  from  := lbl_8;
  to    := lbl_16;
  guard := ( (bleak <= 0) || (2 <= bleak) );
  action:=;
};
transition t_33 :={
  from  := lbl_9;
  to    := lbl_7;
  guard := true;
  action:= bleak' = 0, x' = 0;
};
transition t_36 :={
  from  := lbl_9;
  to    := lbl_7;
  guard := (x <= 9);
  action:= l' = l+1, t' = t+1, x' = x+1;
};
}
strategy dumb {
    Region init := { state = start && true };
}

