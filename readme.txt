The code I have in my final submission is very diffrent from the one I submitted for my p2a.

In this code( the one submitted) I have a thread each fro train which handles the laoding and the crossing, 
whereas the main thread(  the main fucntion  ) handles the dipatching and signalling. In the original idea, the one I 
proposed in p2a, I had no controller thread and only train threads handling everything on itss own.

After some discusion in tuitorials and chats with the TA( VICTOR YOU ARE AWESOME< thank you so much), I realied how doing that is very extra
and how it can be done in a easier and more effieicent way by having a main controller thread.

Here is how I am doing it now:

I have a main thread where I start by intialzing each train struct into a thread.
Once all the thread are created, I send a broadcast to start loading to all threads.
Once broadcast is sent, the train threads tart to load and as soon as they load, they add them into a rpiority queue which i linked list based.
while they are loading themselbes, the main thread waits until there i somehting inside any of the thread.
once it has something inside it, it start to figure out which thread to dipatch and send the signal to 
while the dispatch part is running and train is loaded, the train waits on to a unique signal as I have a unique convar for each train.
once signalled, the train thread then starts to cross while locking the mutex TRACK.
and once finished , it unlock the mutex and the dispatcher function contiues to run untill all trains have been dispatched.

I ue a pthread_join in the end of main thread to wait for each thread to finih their sleep and function before exiting the main fucntiona nd endingn the program.
