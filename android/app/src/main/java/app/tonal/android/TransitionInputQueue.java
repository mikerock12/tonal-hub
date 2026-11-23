package app.tonal.android;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.BooleanSupplier;

/** Bounded single-producer/single-consumer handoff; slots and PCM arrays are reused. */
final class TransitionInputQueue {
    static final int PCM=0,BOUNDARY=1,RESUME=2,END_CAPTURE=3;
    static final class Entry {
        float[] samples;
        int offset,event,positionMs;
        long epoch;
    }
    private final Entry[] slots;
    private final AtomicInteger head=new AtomicInteger(),tail=new AtomicInteger();
    private final Object space=new Object(),work=new Object();
    TransitionInputQueue(int capacity){
        if(capacity<1)throw new IllegalArgumentException("capacity");
        slots=new Entry[capacity];
        for(int i=0;i<capacity;i++)slots[i]=new Entry();
    }
    int capacity(){return slots.length;}
    int size(){return tail.get()-head.get();}
    boolean isEmpty(){return head.get()==tail.get();}
    private boolean isFull(){return size()>=slots.length;}
    boolean offer(float[] samples,int event,int positionMs,long epoch){
        int next=tail.get();
        if(next-head.get()>=slots.length)return false;
        Entry entry=slots[next%slots.length];
        entry.samples=samples;entry.offset=0;entry.event=event;entry.positionMs=positionMs;entry.epoch=epoch;
        tail.lazySet(next+1);
        synchronized(work){work.notifyAll();}
        return true;
    }
    boolean put(float[] samples,int event,int positionMs,long epoch,BooleanSupplier cancelled){
        while(true){
            if(cancelled.getAsBoolean())return false;
            if(offer(samples,event,positionMs,epoch))return true;
            synchronized(space){
                if(cancelled.getAsBoolean())return false;
                if(isFull()){
                    try{space.wait();}catch(InterruptedException interrupted){Thread.currentThread().interrupt();return false;}
                }
            }
        }
    }
    Entry peek(){
        int next=head.get();
        return next==tail.get()?null:slots[next%slots.length];
    }
    void release(Entry entry){
        entry.samples=null;
        head.lazySet(head.get()+1);
        synchronized(space){space.notifyAll();}
    }
    void awaitWork() throws InterruptedException {
        synchronized(work){if(isEmpty())work.wait();}
    }
    void wakeAll(){
        synchronized(space){space.notifyAll();}
        synchronized(work){work.notifyAll();}
    }
}
