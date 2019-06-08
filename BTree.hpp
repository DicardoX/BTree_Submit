# include "utility.hpp"
# include <fstream>
# include <cstddef>
# include "exception.hpp"
# include <cstring>

namespace sjtu {

    const int blockSize = 4096;
    template <class KeyType, class ValueType, class Compare = std::less<KeyType> >

    class BTree {
    public:
        typedef pair <KeyType, ValueType> value_type;
        typedef ssize_t Node_t;
        typedef ssize_t offsetNumber;                       // the offset in the opening file
        class iterator;
        class const_iterator;

    private:
        static const int M = (((sizeof(KeyType) + sizeof(Node_t)) * 2 > 4079) ? 1 : (4079 / (sizeof(Node_t) + sizeof(KeyType)) - 1));                
        static const int L = (((sizeof(value_type)) * 2 > 4076) ? 1 : (4076 / (sizeof(value_type)) - 1));  
        //static const int M = 10;
        //static const int L = 5;
        //static const int M = 2000;
        //static const int L = 400;

        struct nameString {
            char *str;

            nameString() { str = new char[10]; }

            ~nameString() { if (str != nullptr) delete str; }

            void setName(int id) {
                if (id < 0 || id > 9) throw "no more B plus Tree!";
                str[0] = 'd';
                str[1] = 'a';
                str[2] = 't';
                str[3] = static_cast <char> (id + '0');
                str[4] = '.';
                str[5] = 'd';
                str[6] = 'a';
                str[7] = 't';
                str[8] = '\0';
            }

            void setName(char *_str) {
                int i = 0;
                for (; _str[i]; ++i) str[i] = _str[i];
                str[i] = 0;
            }
        };        // the handle of the filename (using other's idea)

        struct leafNode {
            offsetNumber addr;          // the address of the leafNode in the file
            Node_t parent;
            Node_t prev_Node, next_Node;          // previous and next leaf
            int currentSize;                  // number of pairs in leaf
            value_type data[L + 10];
            leafNode() {
                addr = 0;
                parent = 0, prev_Node = 0, next_Node = 0;
                currentSize = 0;
            }
        };
        struct idxNode {
            offsetNumber addr;      	// offset
            Node_t parent;           	// parent
            Node_t children[M + 10];     	// children
            KeyType key[M + 10];   	// key
            int currentSize;              	// number in internal node
            bool childIsLeaf;            	// child is leaf or not
            idxNode() {
                addr = 0, parent = 0;
                for (int i = 0; i <= M; ++i){                    // don't need to initialize the key[]
                    children[i] = 0;
                }
                currentSize = 0;
                childIsLeaf = 0;
            }
        };
        struct treeInfo {
            offsetNumber head;          // head of leaf
            Node_t tail;          // tail of leaf
            Node_t root;          // root of Btree
            size_t size;          // size of Btree
            offsetNumber eof;         // end of file
            treeInfo() {
                head = 0;
                tail = 0;
                root = 0;
                size = 0;
                eof = 0;
            }
        } info;


        /// bpt initialization
        void build_BPlusTree() {
            idxNode root;
            leafNode leaf;
            info.size = 0;
            info.eof = sizeof(treeInfo);

            info.root = root.addr = info.eof;                   // initialization of the tree basic information
            info.eof += sizeof(idxNode);
            info.head = info.tail = leaf.addr = info.eof;
            info.eof += sizeof(leafNode);

            root.parent = 0;                                      // initialization of the origin rootNode
            root.currentSize = 1;
            root.childIsLeaf = true;
            root.children[0] = leaf.addr;

            leaf.parent = root.addr;                            // initialization of the origin leafNode
            leaf.next_Node = leaf.prev_Node = 0;
            leaf.currentSize = 0;

            writeFile(&info, info_offset, sizeof(treeInfo), 1);
            writeFile(&root, root.addr, sizeof(idxNode), 1);
            writeFile(&leaf, leaf.addr, sizeof(leafNode), 1);
        }
        /// the following operations are file operations
        FILE *fp;
        bool fp_open;
        bool file_exist;                   // to judge whether the file exists
        nameString fp_name;
        nameString fp_from_name;
        FILE *fp_from;
        static const int info_offset = 0;

        void openFile() {
            file_exist = true;
            if (!fp_open) {
                fp = fopen(fp_name.str, "rb+");
                if (fp == NULL) {
                    file_exist = false;
                    fp = fopen(fp_name.str, "w");
                    //std::cout<<"create a new file successfully!"<<'\n';
                    fclose(fp);
                    fp = fopen(fp_name.str, "rb+");
                    //std::cout<<"open file successfully!"<<'\n';
                } else readFile(&info, info_offset, 1, sizeof(treeInfo));
                fp_open = true;
            }
        }
        inline void closeFile() {
            if (!fp_open) {
                fclose(fp);
                fp_open = true;
                // std::cout<<"close file successfully!";
            }
        }
        inline void readFile(void *buffer, offsetNumber offset, size_t size, size_t count) const {
            if (fseek(fp, offset, SEEK_SET)) throw "open file failed!";
            fread(buffer, size, count, fp);
        }
        inline void readFile_copy(void *buffer, offsetNumber offset, size_t size, size_t count) const{
            if (fseek(fp_from, offset, SEEK_SET)) throw "open file failed";
            size_t ret = fread(buffer, count, size, fp_from);
        }
        offsetNumber leaf_offset_temp;
        void leaf_copy(offsetNumber offset, offsetNumber from_offset, offsetNumber par_offset) {
            leafNode leaf, from_leaf, pre_leaf;
            readFile_copy(&from_leaf, from_offset, sizeof(leafNode), 1);

            leaf.addr = offset, leaf.parent = par_offset;
            leaf.currentSize = from_leaf.currentSize;
            leaf.prev_Node = leaf_offset_temp;  leaf.next_Node = 0;
            if(leaf_offset_temp != 0){
                readFile(&pre_leaf, leaf_offset_temp, sizeof(leafNode), 1);
                pre_leaf.next_Node = offset;
                writeFile(&pre_leaf, leaf_offset_temp, sizeof(leafNode), 1);
                info.tail = offset;
            }
            else
                info.head = offset;
            for (int i=0; i<leaf.currentSize; ++i){
                leaf.data[i].first = from_leaf.data[i].first;
                leaf.data[i].second = from_leaf.data[i].second;
            }
            writeFile(&leaf, offset, sizeof(leafNode), 1);
            leaf_offset_temp = offset;

            info.eof += sizeof(leafNode);
        }
        void node_copy(offsetNumber offset, offsetNumber from_offset, offsetNumber par_offset) {
            idxNode node, from_node;
            readFile_copy(&from_node, from_offset, sizeof(idxNode), 1);
            writeFile(&node, offset, sizeof(idxNode), 1);
            info.eof += sizeof(idxNode);

            node.offset = offset; node.parent = par_offset;
            node.currentSize = from_node.currentSize; node.childIsLeaf = from_node.childIsLeaf;
            for (int i=0; i<node.currentSize; ++i) {
                node.key[i] = from_node.key[i];
                if(node.childIsLeaf) {
                    leaf_copy(info.eof, from_node.children[i], offset);
                }
                else {
                    node_copy(info.eof, from_node.children[i], offset);
                }
            }
            writeFile(&node, offset, sizeof(idxNode), 1);
        }
        inline void writeFile(void *buffer, offsetNumber offset, size_t size, size_t count) const {
            if (fseek(fp, offset, SEEK_SET)) throw "open file failed!";
            fwrite(buffer, size, count, fp);
        }
        void copyFile(char *to_fileName, char *from_fileName) {
            fp_from_name.setName(from_fileName);
            fp_from = fopen(fp_from_name.str, "rb+");
            if (fp_from == NULL) throw "no such file";

            treeInfo from_info;
            readFile_copy(&from_info, info_offset, sizeof(treeInfo), 1);
            leaf_offset_temp = 0;
            info.size = from_info.size;
            info.root = info.eof = sizeof(treeInfo);
            writeFile(&info, info_offset, sizeof(treeInfo), 1);
            node_copy(info.root, from_info.root, 0);
            writeFile(&info, info_offset, sizeof(treeInfo), 1);
            fclose(fp_from);
        }

        /// when given a key, return the address of the leafNode
        offsetNumber getAddr(const KeyType &key, offsetNumber offset) {
            idxNode index;
            readFile(&index, offset, sizeof(idxNode), 1);
            if(index.childIsLeaf) {
                int pos = 0;
                while(pos < index.currentSize){
                    if(key < index.key[pos]) break;
                    pos++;
                }
                if (pos == 0) return 0;
                return index.children[pos - 1];
            }
            else {
                int pos = 0;
                while(pos < index.currentSize){
                    if(key < index.key[pos]) break;
                    pos++;
                }
                if (pos == 0) return 0;
                return getAddr(key, index.children[pos - 1]);
            }
        }

        void leaf_split(leafNode &leaf, iterator &itr, const KeyType &key) {
            leafNode newLeaf;
            int tmp = leaf.currentSize>>1;
            newLeaf.currentSize = leaf.currentSize - tmp;
            leaf.currentSize = tmp;
            newLeaf.addr = info.eof;
            info.eof += sizeof(leafNode);
            newLeaf.parent = leaf.parent;
            for (int i=0; i<newLeaf.currentSize; ++i)
            {
                newLeaf.data[i].first = leaf.data[i + leaf.currentSize].first;
                newLeaf.data[i].second = leaf.data[i + leaf.currentSize].second;
                if(newLeaf.data[i].first == key)        // let the iterator point to the new leafNode
                {
                    itr.leaf_addr = newLeaf.addr;
                    itr.pos = i;
                }
            }

            newLeaf.next_Node = leaf.next_Node;         // update the near node
            newLeaf.prev_Node = leaf.addr;
            leaf.next_Node = newLeaf.addr;
            leafNode next_leaf;
            if(newLeaf.next_Node == 0)                  // if it is the last node
                info.tail = newLeaf.addr;
            else {
                readFile(&next_leaf, newLeaf.next_Node, sizeof(leafNode), 1);
                next_leaf.prev_Node = newLeaf.addr;
                writeFile(&next_leaf, next_leaf.addr, sizeof(leafNode), 1);
            }

            writeFile(&leaf, leaf.addr, sizeof(leafNode), 1);
            writeFile(&newLeaf, newLeaf.addr, sizeof(leafNode), 1);
            writeFile(&info, info_offset, sizeof(treeInfo), 1);

            idxNode par;                                    // update its parent
            readFile(&par, leaf.parent, sizeof(idxNode), 1);
            node_insert(par, newLeaf.data[0].first, newLeaf.addr);
        }
        void node_split(idxNode &node) {
            idxNode newNode;
            int tmp = node.currentSize>>1;
            newNode.currentSize = node.currentSize - tmp;
            node.currentSize = tmp;
            newNode.parent = node.parent;
            newNode.childIsLeaf = node.childIsLeaf;
            newNode.addr = info.eof;
            info.eof += sizeof(idxNode);
            for (int i = 0; i < newNode.currentSize; ++i){
                newNode.key[i] = node.key[i + node.currentSize];
                newNode.children[i] = node.children[i + node.currentSize];
            }
            // updating  childrens' parents
            leafNode leaf;
            idxNode idx;
            for (int i = 0; i < newNode.currentSize; ++i) {
                if(newNode.childIsLeaf) {
                    readFile(&leaf, newNode.children[i], sizeof(leafNode), 1);
                    leaf.parent = newNode.addr;
                    writeFile(&leaf, leaf.addr, sizeof(leafNode), 1);
                }
                else {
                    readFile(&idx, newNode.children[i], sizeof(idxNode), 1);
                    idx.parent = newNode.addr;
                    writeFile(&idx, idx.addr, sizeof(idxNode), 1);
                }
            }

            if(node.addr == info.root) {				// in this case, we need to generate a new root
                idxNode newRoot;
                newRoot.parent = 0;
                newRoot.childIsLeaf = false;
                newRoot.addr = info.eof;
                info.eof += sizeof(idxNode);
                newRoot.currentSize = 2;
                newRoot.key[0] = node.key[0];
                newRoot.children[0] = node.addr;
                newRoot.key[1] = newNode.key[0];
                newRoot.children[1] = newNode.addr;
                node.parent = newRoot.addr;
                newNode.parent = newRoot.addr;
                info.root = newRoot.addr;

                writeFile(&info, info_offset, sizeof(treeInfo), 1);
                writeFile(&node, node.addr, sizeof(idxNode), 1);
                writeFile(&newNode, newNode.addr, sizeof(idxNode), 1);
                writeFile(&newRoot, newRoot.addr, sizeof(idxNode), 1);
            }
            else {															// if it's not root
                writeFile(&info, info_offset, sizeof(treeInfo), 1);
                writeFile(&node, node.addr, sizeof(idxNode), 1);
                writeFile(&newNode, newNode.addr, sizeof(idxNode), 1);

                idxNode par;                                                // not a root -> need to update its parents
                readFile(&par, node.parent, sizeof(idxNode), 1);
                node_insert(par, newNode.key[0], newNode.addr);
            }
        }
        pair <iterator, OperationResult> leaf_insert(leafNode &leaf, const KeyType &key, const ValueType &value) {
            int pos = 0;
            while(pos < leaf.currentSize) {
                if (key == leaf.data[pos].first)                         // if the key has already a data
                    return pair <iterator, OperationResult> (iterator(nullptr), Fail);
                if (key < leaf.data[pos].first) break;
                pos++;
            }

            for (int i = leaf.currentSize; i > pos; i--){
                leaf.data[i].first = leaf.data[i-1].first;
                leaf.data[i].second = leaf.data[i-1].second;
            }
            leaf.data[pos].first = key;  leaf.data[pos].second = value;
            leaf.currentSize++;
            info.size++;

            iterator itr(this, leaf.addr, pos);                         // return a iterator pointing to the new node with the given key
            writeFile(&info, info_offset, sizeof(treeInfo), 1);
            if(leaf.currentSize <= L)
                writeFile(&leaf, leaf.addr, sizeof(leafNode), 1);
            else
                leaf_split(leaf, itr, key);
            return pair <iterator, OperationResult> (itr, Success);
        }
        void node_insert(idxNode &idx, const KeyType &key, Node_t ch) {
            int pos = 0;
            while(pos < idx.currentSize){
                if (key < idx.key[pos]) break;
                pos++;
            }
            for (int i = idx.currentSize; i > pos; i--){
                idx.key[i] = idx.key[i-1];
                idx.children[i+1] = idx.children[i];
            }
            idx.key[pos] = key;
            idx.children[pos] = ch;
            idx.currentSize++;
            if(idx.currentSize <= M)
                writeFile(&idx, idx.addr, sizeof(idxNode), 1);
            else
                node_split(idx);
        }

    public:
        class iterator {
            friend class BTree;
        private:
            offsetNumber leaf_addr;        // offset of the leaf node
            int pos;							// place of the element in the leaf node
            BTree *from;                  // to use file operations in this class declaration
        public:
            iterator() {
                from = NULL;
                pos = 0; leaf_addr = 0;
            }
            iterator(BTree *defult_from, offsetNumber defult_addr = 0, int defult_pos = 0) {
                from = defult_from;
                leaf_addr = defult_addr;
                pos = defult_pos;
            }
            iterator(const iterator& other) {
                from = other.from;
                leaf_addr = other.leaf_addr;
                pos = other.pos;
            }
            iterator(const const_iterator& other) {
                from = other.from;
                leaf_addr = other.offset;
                pos = other.place;
            }

            // to get the value type pointed by iterator.
            ValueType getValue() {
                leafNode p;
                from -> readFile(&p, leaf_addr, sizeof(leafNode), 1);
                return p.data[pos].second;
            }

            OperationResult modify(const ValueType& value) {
                leafNode p;
                from -> readFile(&p, leaf_addr, sizeof(leafNode), 1);
                p.data[pos].second = value;
                from -> writeFile(&p, leaf_addr, sizeof(leafNode), 1);
                return Success;
            }

            // move to the next element
            iterator operator++(int) {
                iterator itr = *this;
                // end of bptree
                if(*this == from -> end()) {               // if the current place is the end of the tree, return 0
                    from = NULL; pos = 0; leaf_addr = 0;
                    return itr;
                }
                leafNode p;
                from -> readFile(&p, leaf_addr, sizeof(leafNode), 1);
                if(pos == p.currentSize - 1)
                {
                    if(p.next_Node == 0)
                        pos++;
                    else {
                        leaf_addr = p.next_Node;
                        pos = 0;
                    }
                }
                else {
                    pos++;
                }
                return itr;
            }
            iterator& operator++() {
                if(*this == from -> end()) {
                    from = NULL; pos = 0; leaf_addr = 0;
                    return *this;
                }
                leafNode p;
                from -> readFile(&p, leaf_addr, sizeof(leafNode), 1);
                if(pos == p.cnt - 1)
                {
                    if(p.nxt == 0)
                        ++ pos;
                    else {
                        leaf_addr = p.nxt;
                        pos = 0;
                    }
                }
                else{
                    ++ pos;
                }
                return *this;
            }
            iterator operator--(int) {
                iterator itr = *this;
                if(*this == from -> begin()) {
                    from = NULL; pos = 0; leaf_addr = 0;
                    return itr;
                }
                leafNode p, q;
                from -> readFile(&p, leaf_addr, sizeof(leafNode), 1);
                if(pos == 0)
                {
                    leaf_addr = p.prev_Node;
                    from -> readFile(&q, p.prev_Node, sizeof(leafNode), 1);
                    pos = q.currentSize - 1;
                }
                else
                    pos--;
                return itr;
            }
            iterator& operator--() {
                if(*this == from -> begin()) {
                    from = NULL; pos = 0; leaf_addr = 0;
                    return *this;
                }
                leafNode p, q;
                from -> readFile(&p, leaf_addr, sizeof(leafNode), 1);
                if(pos == 0)
                {
                    leaf_addr = p.prev_Node;
                    from -> readFile(&q, p.prev_Node, sizeof(leafNode), 1);
                    pos = q.currentSize - 1;
                }
                else
                    pos--;
                return *this;
            }
            bool operator==(const iterator& rhs) const {
                return (leaf_addr == rhs.leaf_addr && pos == rhs.pos && from == rhs.from);
            }
            bool operator==(const const_iterator& rhs) const {
                return (leaf_addr == rhs.leaf_addr && pos == rhs.pos && from == rhs.from);
            }
            bool operator!=(const iterator& rhs) const {
                return !(*this == rhs);
            }
            bool operator!=(const const_iterator& rhs) const {
                return !(*this == rhs);
            }
        };
        class const_iterator {
            friend class BTree;
        private:
            offsetNumber leaf_addr;        // offset of the leaf node
            int pos;							// place of the element in the leaf node
            const BTree *from;
        public:
            const_iterator() {
                from = NULL;
                pos = 0; leaf_addr = 0;
            }
            const_iterator(const BTree *defult_from, offsetNumber defult_addr = 0, int defult_pos = 0) {
                from = defult_from;
                leaf_addr = defult_addr; pos = defult_pos;
            }
            const_iterator(const iterator& other) {
                from = other.from;
                leaf_addr = other.leaf_addr;
                pos = other.pos;
            }
            const_iterator(const const_iterator& other) {
                from = other.from;
                leaf_addr = other.leaf_addr;
                pos = other.pos;
            }
            // to get the value type pointed by iterator.
            ValueType getValue() {
                leafNode p;
                from -> readFile(&p, leaf_addr, sizeof(leafNode), 1);
                return p.data[pos].second;
            }
            // move to the next element
            const_iterator operator++(int) {
                const_iterator itr = *this;
                if(*this == from -> cend()) {
                    from = NULL; pos = 0; leaf_addr = 0;
                    return itr;
                }
                leafNode p;
                from -> readFile(&p, leaf_addr, sizeof(leafNode), 1);
                if(pos == p.currentSize - 1) {
                    if(p.next_Node == 0)
                        pos++;
                    else {
                        leaf_addr = p.next_Node;
                        pos = 0;
                    }
                } else
                    pos++;
                return itr;
            }
            const_iterator& operator++() {
                if(*this == from -> cend()) {
                    from = NULL; pos = 0; leaf_addr = 0;
                    return *this;
                }
                leafNode p;
                from -> readFile(&p, leaf_addr, sizeof(leafNode), 1);
                if(pos == p.currentSize - 1) {
                    if(p.next_Node == 0)
                        pos++;
                    else {
                        leaf_addr = p.next_Node;
                        pos = 0;
                    }
                } else
                    pos++;
                return *this;
            }
            const_iterator operator--(int) {
                const_iterator itr = *this;
                if(*this == from -> cbegin()) {
                    from = NULL; pos = 0; leaf_addr = 0;
                    return itr;
                }
                leafNode p, q;
                from -> readFile(&p, leaf_addr, sizeof(leafNode), 1);
                if(pos == 0) {
                    leaf_addr = p.prev_Node;
                    from -> readFile(&q, p.prev_Node, sizeof(leafNode), 1);
                    pos = q.currentSize - 1;
                } else
                    pos--;
                return itr;
            }
            const_iterator& operator--() {
                if(*this == from -> cbegin()) {
                    from = nullptr; pos = 0; leaf_addr = 0;
                    return *this;
                }
                leafNode p, q;
                from -> readFile(&p, leaf_addr, sizeof(leafNode), 1);
                if(pos == 0) {
                    leaf_addr = p.prev_Node;
                    from -> readFile(&q, p.prev_Node, sizeof(leafNode), 1);
                    pos = q.currentSize - 1;
                }
                else
                    pos--;
                return *this;
            }
            bool operator==(const iterator& rhs) const {
                return leaf_addr == rhs.offset && pos == rhs.place && from == rhs.from;
            }
            bool operator==(const const_iterator& rhs) const {
                return leaf_addr == rhs.offset && pos == rhs.place && from == rhs.from;
            }
            bool operator!=(const iterator& rhs) const {
                return !(*this == rhs);
            }
            bool operator!=(const const_iterator& rhs) const {
                return !(*this == rhs);
            }
        };
        int ID = 0;

        BTree() {
            fp_name.setName(ID);
            fp = NULL;
            openFile();
            if (!file_exist) build_BPlusTree();
        }

        BTree(const BTree& other) {
            fp_name.setName(ID);
            openFile();
            copyFile(fp_name.str, other.fp_name.str);
        }

        BTree& operator=(const BTree& other) {
            fp_name.setName(ID);
            openFile();
            copyFile(fp_name.str, other.fp_name.str);
        }

        ~BTree() {
            closeFile();
        }

        /**
         * Insert: Insert certain Key-Value into the database
         * Return a pair, the first of the pair is the iterator point to the new
         * element, the second of the pair is Success if it is successfully inserted
         */
        pair <iterator, OperationResult> insert(const KeyType& key, const ValueType& value)
        {
            offsetNumber leaf_offset = getAddr(key, info.root);
            leafNode leaf;

            if(info.size != 0 && leaf_offset != 0)
            {
                readFile(&leaf, leaf_offset, sizeof(leafNode), 1);
                pair <iterator, OperationResult> pr = leaf_insert(leaf, key, value);
                return pr;
            }
            else{					// if the key is point to the smallest element
                readFile(&leaf, info.head, sizeof(leafNode), 1);
                pair <iterator, OperationResult> pr = leaf_insert(leaf, key, value);
                if(pr.second == Fail) return pr;
                offsetNumber offset = leaf.parent;
                idxNode node;
                while(offset != 0) {
                    readFile(&node, offset, sizeof(idxNode), 1);
                    node.key[0] = key;
                    writeFile(&node, offset, sizeof(idxNode), 1);
                    offset = node.parent;
                }
                return pr;
            }
        }

        /**
         * Erase: Erase the Key-Value
         * Return Success if it is successfully erased
         * Return Fail if the key doesn't exist in the database
         */
        OperationResult erase(const KeyType& key) {
            return Fail;
        }

        // Return a iterator pointing to the begining
        iterator begin() {
            return iterator(this, info.head, 0);
        }
        const_iterator cbegin() const {
            return const_iterator(this, info.head, 0);
        }
        // Return a iterator pointing to the end(the next element after the last)
        iterator end() {
            leafNode rear;
            readFile(&rear, info.tail, sizeof(leafNode), 1);
            return iterator(this, info.tail, rear.currentSize);
        }
        const_iterator cend() const {
            leafNode rear;
            readFile(&rear, info.tail, sizeof(leafNode), 1);
            return const_iterator(this, info.tail, rear.currentSize);
        }
        bool empty() const {
            return info.size == 0;
        }
        size_t size() const {
            return info.size;
        }
        void clear() {
            fp = fopen(fp_name.str, "w");
            fclose(fp);
            openFile();
            build_BPlusTree();
        }
        size_t count(const KeyType& key) const {
            return static_cast <size_t> (find(key) != iterator(nullptr));
            //if(find(key) == iterator(NULL))
                //return 0;
            //return 1;
        }
        ValueType at(const KeyType& key){
            iterator itr = find(key);
            leafNode leaf;
            if(itr == end()) {
                throw "not found";
            }
            readFile(&leaf, itr.leaf_addr, sizeof(leafNode), 1);
            return leaf.data[itr.pos].second;
        }
        iterator find(const KeyType& key) {
            offsetNumber leaf_offset = getAddr(key, info.root);
            if(leaf_offset == 0)
                return end();
            leafNode leaf;
            readFile(&leaf, leaf_offset, sizeof(leafNode), 1);
            for (int i = 0; i < leaf.currentSize; i++)
                if (leaf.data[i].first == key)
                    return iterator(this, leaf_offset, i);
            return end();
        }
        const_iterator find(const KeyType& key) const
        {
            offsetNumber leaf_offset = getAddr(key, info_offset);
            if(leaf_offset == 0)
                return cend();
            leafNode leaf;
            readFile(&leaf, leaf_offset, sizeof(leafNode), 1);
            for (int i = 0; i < leaf.currentSize; i++)
                if (leaf.data[i].first == key)
                    return const_iterator(this, leaf_offset, i);
            return cend();
        }
    };
}  // namespace sjtu
