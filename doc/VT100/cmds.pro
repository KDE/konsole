?- consult('dicts.pro').

test :- dict(X,_), check(X), fail; true.
test1 :- findall(A,head(A,_),R), sort(R,Res), print(Res), nl, fail.

check([_,head]) :- !. /* TITLE             */
check([_,emus]) :- !. /* List Word         */
check([_,dflt]) :- !. /* List NumberOrWord */
check([_,sect]) :- !. /* List Word // Dotted Name */
check([_,code]) :- !. /* [ TYP, ("Char"/none/Numb), [arg, ...] ] */
check([_,text]) :- !. /* [ String/nl/ref(Sym) ... ] */

check([_,table,'XPS']) :- !. /* interpretation */

/*Other (type) tables */
/*check([_,table,_]) :- !, print(X), nl.*/

check(X) :- !, print(X), nl.

/* ----------- */
/* State: We're closer to make up a proper data model.
   Todo:
   - put the type/value definitions listed in 'test'
     to a more appropriate place.
   - clearify section material.
   - make a model and a consistency checker.
   - make a report generator.
   - integrate 'TEScreen.C' functions
*/


head(Name,Title)       :- dict([Name,head],Title).
emus(Name,Emus)        :- dict([Name,emus],Emus).
dflt(Name,Defaults)    :- dict([Name,dflt],Defaults).
sect(Name,DottedSect)  :- dict([Name,sect],DottedSect).
code(Name,Code)        :- dict([Name,code],Desc), tcode(Desc,Code).
text(Name,Text)        :- dict([Name,text],Text).

tcode(['PRN',none,[]],prn) :- !.
tcode(['DEL',none,[]],ctl(127)) :- !.
tcode(['CTL',Num,[]],ctl(Num)) :- !.
tcode(['ESC',Chr,[]],esc(Chr)) :- !.
tcode(['HSH',Chr,[]],esc(Chr)) :- !.
tcode(['CSI',Chr,[P,'...']],csi(Chr,list(P))) :- !.
tcode(['CSI',Chr,Parm],csi(Chr,Parm)) :- !.
tcode(['PRI',Chr,[P,'...']],pri(Chr,list(P))) :- !.
tcode(['PRI',Chr,Parm],pri(Chr,Parm)) :- !.
tcode(['SCS',none,[A,B]],scs([A,B])) :- !.
tcode(['VT5',none,[X,Y]],vt5([X,Y])) :- !.
tcode(P,P) :- writef("\n - fail\n %t \n\n",[P]).

pheads :-
  head(N,T),
  writef("%w - %s\n",[N,T]),
  fail; true.

pcodes :-
  code(N,P),
  writef("%w - %t\n",[N,P]),
  fail; true.

