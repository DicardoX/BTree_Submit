#include "utility.hpp"
#include <functional>
#include <cstddef>
#include "exception.hpp"
#include <map>
namespace sjtu {
    const int blockSize = 4096;
    int ID;
    template <class Key, class Value, class Compare = std::less<Key> >
    class BTree {
    public:
        typedef pair<Key, Value> value_type;
        typedef ssize_t Node_t;
        typedef ssize_t offsetNumber;
        class iterator;
        class const_iterator;
    private:
        const int bpt_info_offset = 0;                  // the address of the basic information
        static const int M = blockSize / sizeof(Key);
        static const int L = sizeof(value_type) > blockSize ? 1 : blockSize / sizeof(value_type);
        //static const int M = 2000;
        //static const int L = 300;

        struct fileName{
            char *name;
            fileName(){name = new char[20];}
            ~fileName(){if(name) delete name;}

            void nameName(int ID){
                name[0] = 'd';
                name[1] = 'a';
                name[2] = 't';
                name[3] = '0';
                name[4] = '.';
                name[5] = 'd';
                name[6] = 'a';
                name[7] = 't';
                name[8] = '\0';
            }
        };

        struct bpt_Info{
            Node_t root;   //rootNode of the tree
            Node_t head;   //firstNode of the leaf
            Node_t tail;   //lastNode of the leaf
            size_t size;    //size of leaf of the tree
            ssize_t eof;
            bpt_Info(){
                root = 0;
                head = 0;
                tail = 0;
                size = 0;
                eof = 0;
            }
        };
        struct internalNode{
            ssize_t offset; //address in file
            ssize_t parent;
            ssize_t child[M + 1];
            Key key[M + 1]; //key
            int num;
            bool type;
            internalNode(){
                num = 0;
                offset = 0;
                parent = 0;
                type = 1;
            }
        };

        struct leafNode{
            ssize_t offset;
            ssize_t parent;
            ssize_t prev;
            ssize_t next;
            int num;      //number of the pair(key,value)
            value_type data[L + 1];      //data array
            leafNode(){
                offset = 0;
                parent = 0;
                prev = 0;
                next = 0;
                num = 0;
            }
        };

        FILE *fp;
        fileName fp_name;
        bpt_Info info;
        bool fp_open;

        bool exist;

        void openFile(){
            //std::cout<<"file opening\n";
            if(!fp_open){
                //std::cout<<"file hasn't benn opened\n";
                fp = fopen(fp_name.name,"rb+");
                if(fp == NULL){
                    fp = fopen(fp_name.name,"w");
                    fclose(fp);
                    fp = fopen(fp_name.name,"rb+");
                    exist = false;
                }
                else{
                    exist = true;
                    readFile(&info,bpt_info_offset,1, sizeof(bpt_Info));
                }
                fp_open = true;
            }
            //std::cout<<"open file successully!\n";
        }

        void closeFile(){
            if(fp_open){
                fclose(fp);
                fp_open = false;
            }
        }

        void readFile(void* place,ssize_t offset,size_t num,size_t size){

            if(fseek(fp,offset,SEEK_SET)) ;
                //std::cout<<"read-failure\n";   ///something should be done?I don't know.
            fread(place,size,num,fp);
        }

        void write(void* place,ssize_t offset,size_t num,size_t size){
            //std::cout<<"write operating\n";
            if(fseek(fp,offset,SEEK_SET));             // throw "error occured";
            fwrite(place,size,num,fp);
            //std::cout<<"write-operation complete\n";
        }

        void build_BTree(){
            if(exist){
                //read(&info,bpt_info_offset,1, sizeof(bpt_Info));
                return;
            }
            //std::cout<<"initializing\n";
            info.size = 0;
            info.eof = bpt_info_offset;
            info.eof += sizeof(bpt_Info);
            internalNode root;
            info.root = root.offset = info.eof;
            info.eof += sizeof(internalNode);
            leafNode leaf;
            info.head = info.tail = leaf.offset = info.eof;
            info.eof += sizeof(leafNode);

            root.num = 0;
            root.type = 1;
            root.parent = 0;
            //std::cout<<"leaf.offset is "<<leaf.offset<<std::endl;
            root.child[0] = leaf.offset;

            leaf.parent = root.offset;
            leaf.prev = leaf.next = 0;
            leaf.num = 0;

            write(&info,bpt_info_offset,1, sizeof(bpt_Info));
            write(&root,root.offset,1, sizeof(internalNode));
            write(&leaf,leaf.offset,1, sizeof(leafNode));
        }

        offsetNumber getAddr(const Key &key,ssize_t offset){

            internalNode tmp;
            readFile(&tmp,offset,1, sizeof(internalNode));

            if(tmp.type == 1){
                int pos = 0;
                //std::cout<<"tmp.num is "<<tmp.num<<'\n';
                while(pos < tmp.num && key >= tmp.key[pos + 1])
                    pos++;
                return tmp.child[pos];
            }
            else{
                int pos = 0;
                while(pos < tmp.num && key >= tmp.key[pos + 1])
                    pos++;
                return getAddr(key,tmp.child[pos]);
            }
        }

        std::pair<iterator,OperationResult> insert_leaf(leafNode &leaf,const Key &key,const Value &value){
            //std::cout<<"in insert_leaf operation:\n";
            int pos = 0;
            //std::cout<<"leaf.num is "<<leaf.num<<'\n';
            while(pos < leaf.num && key > leaf.data[pos].first)
                pos++;
            for(int i = leaf.num - 1;i >= pos;i--){
                leaf.data[i + 1].first = leaf.data[i].first;
                leaf.data[i + 1].second = leaf.data[i].second;
                //leaf.data[i + 1] = leaf.data[i];
            }
            leaf.data[pos].first = key;
            leaf.data[pos].second = value;
            leaf.num++;
            info.size++;
            iterator result(this,leaf.offset,pos);
            write(&info,bpt_info_offset,1, sizeof(bpt_Info));
            if(leaf.num <= L - 1){
                write(&leaf,leaf.offset,1, sizeof(leafNode));
            }
            else{
                split_leaf(leaf,result,key);
            }
            return std::pair<iterator,OperationResult>(result,Success);
        }
        void insert_node(internalNode &internal,const Key &key,const ssize_t child){
            int pos = 1;
            while(pos <= internal.num && key >= internal.key[pos])
                pos++;
            for(int i = internal.num;i >= pos;i--){
                internal.key[i + 1] = internal.key[i];
                internal.child[i + 1] = internal.child[i];
            }
            internal.key[pos] = key;
            internal.child[pos] = child;
            internal.num++;
            //std::cout<<"internal.num is "<<internal.num<<'\n';
            if(internal.num < M - 1){
                write(&internal,internal.offset,1, sizeof(internalNode));
            }
            else{
                split_node(internal);
            }
        }

        void split_leaf(leafNode &leaf,iterator &it,const Key &key){
            leafNode newLeaf;
            newLeaf.num = leaf.num - (leaf.num / 2);
            leaf.num /= 2;

            newLeaf.offset = info.eof;
            newLeaf.parent = leaf.parent;
            newLeaf.next = leaf.next;
            newLeaf.prev = leaf.offset;
            leaf.next = newLeaf.offset;

            info.eof += sizeof(leafNode);

            for(int i = 0;i < newLeaf.num;i++){
                newLeaf.data[i].first = leaf.data[i + leaf.num].first;
                newLeaf.data[i].second = leaf.data[i + leaf.num].second;
                if(newLeaf.data[i].first == key){
                    it.offset = newLeaf.offset;
                    it.pos = i;
                }
            }

            if(newLeaf.next == 0){
                info.tail = newLeaf.offset;
            }
            else{
                leafNode next_leaf;
                readFile(&next_leaf,newLeaf.next,1, sizeof(leafNode));
                next_leaf.prev = newLeaf.offset;
                write(&next_leaf,newLeaf.next,1, sizeof(leafNode));
            }

            write(&info,bpt_info_offset,1, sizeof(bpt_Info));
            write(&leaf,leaf.offset,1, sizeof(leafNode));
            write(&newLeaf,newLeaf.offset,1,sizeof(leafNode));

            internalNode parent;
            //std::cout<<"leaf.parent is "<<leaf.parent<<std::endl;
            readFile(&parent,leaf.parent,1, sizeof(internalNode));
            insert_node(parent,newLeaf.data[0].first,newLeaf.offset);
        }

        void split_node(internalNode &internal){
            //std::cout<<"call split_node function\n";
            if(internal.offset != info.root) {
                internalNode newnode;
                newnode.offset = info.eof;
                info.eof += sizeof(internalNode);

                newnode.num = internal.num - (internal.num / 2) - 1;
                internal.num /= 2;
                newnode.parent = internal.parent;
                newnode.type = internal.type;
                //std::cout<<"another.num is "<<another.num<<'\n';
                //std::cout<<"internal.num is "<<internal.num<<'\n';


                for (int i = 0;i <= newnode.num;i++){
                    newnode.child[i] = internal.child[i + 1 + internal.num];
                }
                for(int i = 1;i <= newnode.num;i++){
                    newnode.key[i] = internal.key[i + 1 + internal.num];
                }
                write(&internal,internal.offset,1, sizeof(internalNode));
                write(&newnode,newnode.offset,1,sizeof(internalNode));

                //change children's parent pointer
                internalNode tmp;
                leafNode leaf;
                for(int i = 0;i <= newnode.num;i++){
                    if(newnode.type == 1){
                        //if its child is leaf
                        readFile(&leaf,newnode.child[i],1, sizeof(leafNode));
                        leaf.parent = newnode.offset;
                        write(&leaf,newnode.child[i],1, sizeof(leafNode));
                    }
                    else{
                        //if its child isn't leaf
                        readFile(&tmp,newnode.child[i],1, sizeof(internalNode));
                        tmp.parent = newnode.offset;
                        write(&tmp,newnode.child[i],1, sizeof(internalNode));
                    }
                }

                //insert key and children pointer in parent node
                readFile(&tmp,internal.parent,1, sizeof(internalNode));
                insert_node(tmp,internal.key[internal.num + 1],newnode.offset);
            }
            else{
                internalNode new_root;
                internalNode half;
                new_root.offset = info.eof;
                info.eof += sizeof(internalNode);
                half.offset = info.eof;
                info.eof += sizeof(internalNode);
                info.root = new_root.offset;
                internal.parent = info.root;
                half.parent = info.root;
                half.num = internal.num - internal.num / 2 - 1;
                for(int i = 1;i <= half.num;i++){
                    half.key[i] = internal.key[i + internal.num / 2 + 1];
                }
                for(int i = 0;i <= half.num;i++){
                    half.child[i] = internal.child[i + internal.num / 2 + 1];
                }

                new_root.num = 1;
                new_root.parent = 0;
                new_root.type = 0;
                half.type = internal.type;
                new_root.child[0] = internal.offset;
                new_root.child[1] = half.offset;
                new_root.key[1] = internal.key[internal.num / 2 + 1];
                internal.num /= 2;

                if(internal.type){
                    //    std::cout<<"root's child is leaf\n";
                    leafNode change;

                    for(int i = 0;i <= half.num;i++){
                        readFile(&change,internal.child[i + internal.num + 1],1, sizeof(leafNode));
                        change.parent = half.offset;
                        write(&change,internal.child[i + internal.num + 1],1, sizeof(leafNode));
                    }
                }
                else{
                    internalNode change;
                    for(int i = 0;i <= half.num;i++){
                        readFile(&change,internal.child[i + internal.num + 1],1, sizeof(internalNode));
                        change.parent = half.offset;
                        write(&change,internal.child[i + internal.num + 1],1, sizeof(internalNode));
                    }
                }

                write(&info,bpt_info_offset,1, sizeof(bpt_Info));
                write(&internal,internal.offset,1, sizeof(internalNode));
                write(&new_root,new_root.offset,1, sizeof(internalNode));
                write(&half,half.offset,1, sizeof(internalNode));

                return;
            }
        }

    public:
        class iterator {
            friend class BTree;
        private:
            BTree* tree;
            ssize_t offset;
            int pos;
        public:
            bool modify(const Value& value){
                leafNode tmp;
                tree->read(&tmp,offset,1, sizeof(leafNode));
                tmp.data[pos].second = value;
                tree->write(&tmp,offset,1, sizeof(leafNode));
                return true;
            }
            iterator(BTree*t = NULL,ssize_t off = 0,int p = 0) : tree(t),offset(off),pos(p){}
            iterator(const iterator& other) {
                tree = other.tree;
                offset = other.offset;
                pos = other.pos;
            }
            // Return a new iterator which points to the n-next elements
            iterator operator++(int) {
                iterator pre = *this;
                if(pre == tree->end()){
                    tree = NULL;
                    offset = 0;
                    pos = 0;
                    return pre;
                }

                leafNode tmp;
                tree->read(&tmp,offset,1,sizeof(leafNode));
                if(pos == tmp.num - 1){
                    offset = tmp.next;
                    pos = 0;
                }
                else{
                    pos++;
                }
                return pre;
            }
            iterator& operator++() {
                if(*this == tree->end()){
                    return iterator();
                }

                leafNode tmp;
                tree->read(&tmp,offset,1,sizeof(leafNode));
                if(pos == tmp.num - 1){
                    offset = tmp.next;
                    pos = 0;
                }
                else{
                    pos++;
                }
                return *this;
            }
            iterator operator--(int) {
                iterator pre = *this;
                if(pre == tree->begin()){
                    tree = NULL;
                    offset = 0;
                    pos = 0;
                    return pre;
                }

                leafNode tmp;
                tree->read(&tmp,offset,1, sizeof(leafNode));

                if(pos == 0){
                    offset = tmp.prev;
                    tree->read(&tmp,tmp.prev,1, sizeof(leafNode));
                    pos =  tmp.num - 1;
                }
                else{
                    pos--;
                }

                return pre;
            }
            iterator& operator--() {
                if(*this == tree->begin()){
                    tree = NULL;
                    offset = 0;
                    pos = 0;
                    return *this;
                }

                leafNode tmp;
                tree->read(&tmp,offset,1, sizeof(leafNode));

                if(pos == 0){
                    offset = tmp.prev;
                    tree->read(&tmp,tmp.prev,1, sizeof(leafNode));
                    pos =  tmp.num - 1;
                }
                else{
                    pos--;
                }
                return *this;
            }
            // Overloaded of operator '==' and '!='
            // Check whether the iterators are same
            bool operator==(const iterator& rhs) const {
                return tree == rhs.tree && offset == rhs.offset
                       && pos == rhs.pos;
            }
            bool operator==(const const_iterator& rhs) const {
                return tree == rhs.tree && offset == rhs.offset
                       && pos == rhs.pos;
            }
            bool operator!=(const iterator& rhs) const {
                return tree != rhs.tree || offset != rhs.offset
                       || pos != rhs.pos;
            }
            bool operator!=(const const_iterator& rhs) const {
                return tree != rhs.tree || offset != rhs.offset
                       || pos != rhs.pos;
            }
            long long getvalue(){
                leafNode leaf;
                tree->read(&leaf,offset,1, sizeof(leafNode));
                return leaf.data[pos].second;
            }
        };
        class const_iterator {
            friend class BTree;
        private:
            BTree* tree;
            ssize_t offset;
            int pos;
        public:
            const_iterator(BTree*t = NULL,ssize_t off = 0,int p = 0) : BTree(t),offset(off),pos(p){}
            const_iterator(const const_iterator& other) {
                tree = other.tree;
                offset = other.offset;
                pos = other.pos;
            }
            const_iterator(const iterator& other) {
                tree = other.tree;
                offset = other.offset;
                pos = other.pos;
            }
            const_iterator operator++(int) {
                const_iterator pre = *this;
                if(pre == tree->end()){
                    tree = NULL;
                    offset = 0;
                    pos = 0;
                    return pre;
                }

                leafNode tmp;
                tree->read(&tmp,offset,1,sizeof(leafNode));
                if(pos == tmp.num - 1){
                    offset = tmp.next;
                    pos = 0;
                }
                else{
                    pos++;
                }
                return pre;
            }
            const_iterator& operator++() {
                if(*this == tree->end()){
                    return iterator();
                }

                leafNode tmp;
                tree->read(&tmp,offset,1,sizeof(leafNode));
                if(pos == tmp.num - 1){
                    offset = tmp.next;
                    pos = 0;
                }
                else{
                    pos++;
                }
                return *this;
            }
            const_iterator operator--(int) {
                const_iterator pre = *this;
                if(pre == tree->begin()){
                    tree = NULL;
                    offset = 0;
                    pos = 0;
                    return pre;
                }

                leafNode tmp;
                tree->read(&tmp,offset,1, sizeof(leafNode));

                if(pos == 0){
                    offset = tmp.prev;
                    tree->read(&tmp,tmp.prev,1, sizeof(leafNode));
                    pos =  tmp.num - 1;
                }
                else{
                    pos--;
                }

                return pre;
            }
            const_iterator& operator--() {
                if(*this == tree->begin()){
                    tree = NULL;
                    offset = 0;
                    pos = 0;
                    return *this;
                }

                leafNode tmp;
                tree->read(&tmp,offset,1, sizeof(leafNode));

                if(pos == 0){
                    offset = tmp.prev;
                    tree->read(&tmp,tmp.prev,1, sizeof(leafNode));
                    pos =  tmp.num - 1;
                }
                else{
                    pos--;
                }

                return *this;
            }
            // Overloaded of operator '==' and '!='
            // Check whether the iterators are same
            bool operator==(const iterator& rhs) const {
                return tree == rhs.tree && offset == rhs.offset
                       && pos == rhs.pos;
            }
            bool operator==(const const_iterator& rhs) const {
                return tree == rhs.tree && offset == rhs.offset
                       && pos == rhs.pos;
            }
            bool operator!=(const iterator& rhs) const {
                return tree != rhs.tree || offset != rhs.offset
                       || pos != rhs.pos;
            }
            bool operator!=(const const_iterator& rhs) const {
                return tree != rhs.tree || offset != rhs.offset
                       || pos != rhs.pos;
            }
        };
        BTree() {
            fp_name.nameName(ID);
            fp = nullptr;
            fp_open = false;
            //std::cout<<M<<'\t'<<L<<std::endl;
            //std::cout<<"constructing\n";
            openFile();
            build_BTree();
        }
        BTree(const BTree& other) {
            // Todo Copy
        }
        BTree& operator=(const BTree& other) {
            // Todo Assignment
        }
        ~BTree() {
            closeFile();
        }
        // Insert: Insert certain Key-Value into the database
        // Return a pair, the first of the pair is the iterator point to the new
        // element, the second of the pair is Success if it is successfully inserted
        pair<iterator, OperationResult> insert(const Key& key, const Value& value) {
            openFile();
            ssize_t offset = getAddr(key,info.root);
            leafNode leaf;
            readFile(&leaf,offset,1, sizeof(leafNode));
            std::pair<iterator,OperationResult> tmp;
            tmp = insert_leaf(leaf,key,value);

            pair<iterator,OperationResult> result;
            result.first = tmp.first;
            result.second = tmp.second;

            return result;
        }
        // Erase: Erase the Key-Value
        // Return Success if it is successfully erased
        // Return Fail if the key doesn't exist in the database
        OperationResult erase(const Key& key) {
            // TODO erase function
            return Success;  // If you can't finish erase part, just remaining here.
        }
        // Return a iterator to the beginning
        iterator begin() {
            iterator tmp(this,info.head,0);
            return tmp;
        }
        const_iterator cbegin() const {
            const_iterator tmp(this,info.head,0);
            return tmp;
        }
        // Return a iterator to the end(the next element after the last)
        iterator end() {
            leafNode tail;
            readFile(&tail,info.tail,1, sizeof(leafNode));
            iterator tmp(this,tail.offset,tail.num);
            return tmp;
        }
        const_iterator cend() const {
            leafNode tail;
            read(&tail,info.tail,1, sizeof(leafNode));
            const_iterator tmp(this,tail.offset,tail.num);
            return tmp;
        }
        // Check whether this BTree is empty
        bool empty() const {
            return info.size == 0;
        }
        // Return the number of <K,V> pairs
        size_t size() const {
            return info.size;
        }
        // Clear the BTree
        void clear() {
            fp = fopen(fp_name.str,"w");
            fclose(fp);
            build_BTree();
        }
        // Return the value refer to the Key(key)
        Value at(const Key& key){
            iterator pt = find(key);
            if(pt.tree != NULL){
                leafNode leaf;
                readFile(&leaf,pt.offset,1, sizeof(leafNode));
                int pos = pt.pos;
                return leaf.data[pos].second;
            }
        }
        /**
         * Returns the number of elements with key
         *   that compares equivalent to the specified argument,
         * The default method of check the equivalence is !(a < b || b > a)
         */
        size_t count(const Key& key) const {}
        /**
         * Finds an element with key equivalent to key.
         * key value of the element to search for.
         * Iterator to an element with key equivalent to key.
         *   If no such element is found, past-the-end (see end()) iterator is
         * returned.
         */
        iterator find(const Key& key) {
            ssize_t offset = getAddr(key,info.root);
            leafNode leaf;
            readFile(&leaf,offset,1, sizeof(leafNode));
            for(int i = 0;i < leaf.num;i++){
                if(leaf.data[i].first == key){
                    //std::cout<<"i is "<<i<<" key is "<<leaf.data[i].first<<" value is "<<leaf.data[i].second<<std::endl;
                    return iterator(this,offset,i);
                }

            }
            return end();
        }
        const_iterator find(const Key& key) const {
            //read(&info,bpt_info_offset,1, sizeof(bpt_Info));
            ssize_t offset = find_leaf(key,info.root);
            leafNode leaf;
            read(&leaf,offset,1, sizeof(leafNode));
            for(int i = 0;i < leaf.num;i++){
                if(leaf.data[i].first == key){
                    return const_iterator(this,offset,i);
                }

            }
            return cend();
        }
    };

}  // namespace sjtu
