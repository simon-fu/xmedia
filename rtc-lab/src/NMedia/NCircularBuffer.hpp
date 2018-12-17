

#ifndef NCircularBuffer_hpp
#define NCircularBuffer_hpp

#include <stdio.h>
#include <functional>

// see https://embeddedartistry.com/blog/2017/4/6/circular-buffers-in-cc#c-
// see https://github.com/embeddedartistry/embedded-resources/blob/master/examples/cpp/circular_buffer.cpp
// see https://github.com/rlogiacco/CircularBuffer/blob/master/CircularBuffer.tpp

typedef size_t NCircularBufferSizeT ;
template<typename T>
class NCircularBuffer {
public:
    typedef T                                       value_type;
    typedef value_type&                             reference;
    typedef const value_type&                       const_reference;
    typedef NCircularBufferSizeT                    size_type;
public:
    
    NCircularBuffer(T *buf, NCircularBufferSizeT capacity)
    :buffer_(buf), capacity_(capacity)
    , head_(buffer_), tail_(buffer_), count_(0){
        
    }
    
    virtual ~NCircularBuffer(){
        
    }
    
    /**
     * Adds an element to the beginning of buffer;
     * returns `false` if the addition caused overwriting an existing element.
     */
    bool pushFront(value_type&& value) {
        bool empty_slot = true;
        empty_slot = unshiftFront();
        front() = value;
        return empty_slot;
    }
    
    bool unshiftFront() {
        if (head_ == buffer_) {
            head_ = buffer_ + capacity_;
        }
        --head_;
        if (count_ == capacity_) {
            if (tail_-- == buffer_) {
                tail_ = buffer_ + capacity_ - 1;
            }
            return  false;
        } else {
            if (count_++ == 0) {
                tail_ = head_;
            }
            return true;
        }
    }
    
    bool unshiftFront(size_type num, const_reference value) {
        bool empty_slot = true;
        for(size_type n = 0; n < num; ++n){
            empty_slot = pushFront(value);
        }
        return empty_slot;
    }
    
    bool unshiftFront(size_type num) {
        bool empty_slot = true;
        for(size_type n = 0; n < num; ++n){
            empty_slot = unshiftFront();
        }
        return empty_slot;
    }
    
    
    /**
     * Adds an element to the end of buffer;
     * returns `false` if the addition caused overwriting an existing element.
     */
    size_type pushBack(const_reference value){
        auto over_num = shiftBack();
        back() = value;
        return over_num;
    }
    
    
    size_type shiftBack(){
        if (++tail_ == buffer_ + capacity_) {
            tail_ = buffer_;
        }
        
        if (count_ == capacity_) {
            if (++head_ == buffer_ + capacity_) {
                head_ = buffer_;
            }
            return 1;
        } else {
            if (count_++ == 0) {
                head_ = tail_;
            }
            return 0;
        }
    }
    
    size_type shiftBack(size_type num, const_reference value) {
        size_type over_num = 0;
        for(size_type n = 0; n < num; ++n){
            over_num += pushBack(value);
        }
        return over_num;
    }
    
    size_type shiftBack(size_type num) {
        size_type over_num = 0;
        for(size_type n = 0; n < num; ++n){
            over_num += shiftBack();
        }
        return over_num;
    }
    
    
    /**
     * Removes an element from the beginning of the buffer.
     */
    void popFront(){
        shiftFront();
    }
    
    void popFront(size_type num){
        for(size_type n = 0; n < num; ++n){
            shiftFront();
        }
    }
    
    reference shiftFront(){
        reference ref = *head_;
        if (count_ > 0) {
            head_++;
            if (head_ >= buffer_ + capacity_) {
                head_ = buffer_;
            }
            count_--;
        }else{
            //void(* crash) (void) = 0;
            //crash();
        }
        return ref;
    }
    
    
    /**
     * Removes an element from the end of the buffer.
     */
    void popBack(){
        unshiftBack();
    }
    
    void popBack(size_type num){
        for(size_type n = 0; n < num; ++n){
            unshiftBack();
        }
    }
    
    reference unshiftBack(){
        reference ref = *tail_;
        if (count_ > 0) {
            tail_--;
            if (tail_ < buffer_) {
                tail_ = buffer_ + capacity_ - 1;
            }
            count_--;
        }else{
            //void(* crash) (void) = 0;
            //crash();
        }
        return ref;
    }
    
    /**
     * Returns the element at the beginning of the buffer.
     */
    reference inline front() {
        return *head_;
    }
    
    const_reference inline front() const{
        return *head_;
    }
    
    /**
     * Returns the element at the end of the buffer.
     */
    reference inline back() {
        return *tail_;
    }
    
    const_reference inline back() const{
        return *tail_;
    }
    
    /**
     * Array-like access to buffer
     */
    inline
    reference operator [] (NCircularBufferSizeT index) {
        return *(buffer_ + ((head_ - buffer_ + index) % capacity_));
    }
    
    inline
    const_reference operator [] (NCircularBufferSizeT index) const{
        return *(buffer_ + ((head_ - buffer_ + index) % capacity_));
    }
    
    /**
     * Returns how many elements are actually stored in the buffer.
     */
    NCircularBufferSizeT inline size() const{
        return count_;
    }
    
    /**
     * Returns how many elements can be safely pushed into the buffer.
     */
    NCircularBufferSizeT inline available() const{
        return capacity_ - count_;
    }
    
    /**
     * Returns how many elements can be potentially stored into the buffer.
     */
    NCircularBufferSizeT inline capacity() const{
        return capacity_;
    }
    
    /**
     * Returns `true` if no elements can be removed from the buffer.
     */
    bool inline isEmpty() const{
        return count_ == 0;
    }
    
    /**
     * Returns `true` if no elements can be added to the buffer without overwriting existing elements.
     */
    bool inline isFull() const{
        return count_ == capacity_;
    }
    
    /**
     * Resets the buffer to a clean status, making all buffer positions available.
     */
    void inline clear() {
        head_ = tail_ = buffer_;
        count_ = 0;
    }
    
    typedef  std::function<int (NCircularBufferSizeT , reference )> TraverseFuncT;
    inline int traverse (NCircularBufferSizeT startIndex
                         , NCircularBufferSizeT num
                         , const TraverseFuncT& func) {
        
        for(NCircularBufferSizeT i = startIndex, n = 0
            ; i < size() && n < num
            ; ++i, ++n){
            int state = func(n, (*this)[i]);
            if(state != 0){
                return state;
            }
        }
        return 0;
    }
    
    inline int traverse (NCircularBufferSizeT startIndex
                         , const TraverseFuncT& func) {
        
        for(NCircularBufferSizeT i = startIndex, n = 0
            ; i < size()
            ; ++i, ++n){
            int state = func(n, (*this)[i]);
            if(state != 0){
                return state;
            }
        }
        return 0;
    }
    
protected:
    T *buffer_;
    T *head_;
    T *tail_;
    NCircularBufferSizeT capacity_;
    NCircularBufferSizeT count_;
    
};

template<typename T>
class NCircularVector : public NCircularBuffer<T> {
public:
    NCircularVector(NCircularBufferSizeT capacity)
    :NCircularBuffer<T>(
                        (capacity > 0)?new T[capacity]:nullptr,
                        capacity){
        
    }
    
    NCircularVector(const NCircularVector&other)
    :NCircularVector(other.capacity_){
        
    }
    
    virtual ~NCircularVector(){
        delete[] this->buffer_;
    }
};

template<typename T, NCircularBufferSizeT S>
class NCircularArray : public NCircularBuffer<T> {
public:
    NCircularArray():NCircularBuffer<T>(buffer_, S){
        
    }
private:
    T buffer_[S];
};

#endif /* NCircularBuffer_hpp */
