#include <memory>
#include <string>
#include <map>
#include <iostream>

namespace Radix {
class Node;

class Tree {
private:
  std::string matchedPrefix(std::string str1, std::string str2) const;

protected:
  std::map<std::string, std::unique_ptr<Node>> childrens;

public:
  void add(std::unique_ptr<Node> node, std::string text);
  Node *search(std::string text) const;
  void remove(std::string text);
  // Returns the reference of the children map
  // This returns reference cause std::unique_ptr can't be copied
  // Warning: don't try to modify the returned map
  std::map<std::string, std::unique_ptr<Node>> *getChildrens() {
    return &this->childrens;
  }
  ulong totalChildNodes() const {
    return this->childrens.size();
  }
};

class Node : public Tree {
private:
  ulong addressInRocksDB = 0;
  bool validNode = false;

public:
  Node() : validNode(false) {}
  Node(ulong address) : addressInRocksDB(address), validNode(true) {}

  void addAddress(ulong address) {
    this->addressInRocksDB = address;
    this->validNode = true;
  }

  // Returns the address of the data in RocksDB
  ulong get() const {
    return this->addressInRocksDB;
  }

  // Return true if the node itself doesn't hold any data
  bool isNull() {
    return !this->validNode;
  }
};

std::string Tree::matchedPrefix(std::string str1, std::string str2) const {
  std::size_t i, smallerLength;
  std::string matchedPart;

  // Finding for smaller string length
  if (str1.length() < str2.length()) {
    smallerLength = str1.length();
  } else {
    smallerLength = str2.length();
  }

  for (i = 0; i < smallerLength; ++i) {
    if (str1[i] != str2[i])
      break;
    matchedPart += str1[i];
  }

  return matchedPart;
}

void Tree::add(std::unique_ptr<Node> node, std::string text) {
  // If there's is no child node
  // Store the text directly as the node
  if (this->childrens.empty()) {
    this->childrens[text] = std::move(node);
    return;
  }

  // If at least one child node exist
  for (auto &key : this->childrens) {
    std::string matched = matchedPrefix(key.first, text);

    // if matched prefix is same as the input text
    // Then make the old matched node as it's child node and unmatched part of the
    // string should be new value of old matched node
    if (!matched.empty() && matched.compare(text) == 0) {
      auto oldNode = std::move(key.second);

      node->add(std::move(oldNode), key.first.substr(text.length()));
      this->childrens.erase(key.first);
      this->childrens[text] = std::move(node);
      return;
    }
    // Check if any key's prefix matched with the text, if yes then create a new key with the matched prefix
    // and then make a child node with the unmatched part of the string, and also move existing node as child
    // node of the prefix matched node
    else if (!matched.empty() && matched.compare(key.first) != 0) {
      auto newNode = std::make_unique<Node>();
      auto oldNode = std::move(key.second);

      // Getting remaining part of the string excluding the prefix
      newNode->add(std::move(node), text.substr(matched.length()));
      newNode->add(std::move(oldNode), key.first.substr(matched.length()));
      this->childrens.erase(key.first);
      this->childrens[matched] = std::move(newNode); // Storing matched Prefix node
      return;
    }
    // If the node total keyname is matched with the matched part,
    // Then look for child node and check the matched substring
    else if (matched.compare(key.first) == 0) {
      std::string remainingText = text.substr(key.first.length());
      std::string newKey = key.first;
      Node *rootNode = key.second.get();

      // Getting lowest matched Node
      while (matched.compare(newKey) == 0) {
        for (auto &subkey : rootNode->childrens) {
          matched = matchedPrefix(subkey.first, remainingText);
          if (matched.compare(subkey.first) == 0) {
            newKey = subkey.first;
            rootNode = subkey.second.get();
            remainingText = remainingText.substr(matched.length());
            break;
          } else {
            matched = "";
          }
        }
      }

      // Storing the node under the common matched node
      rootNode->add(std::move(node), remainingText);
      return;
    }
  }

  // If at least one child node exist but no prefix matched then simply add the node
  this->childrens[text] = std::move(node);
}

// Searches for the Node into the Tree
// if Node doesn't exist then returns nullptr
Node *Tree::search(std::string text) const {
  size_t start_pos = 0;
  size_t length = 1;
  std::string findStr;
  Node *current = nullptr;

  // Start searching from the root level
  while (start_pos == 0 && length <= text.size()) {
    auto it = this->childrens.find(text.substr(0, length));
    if (it != this->childrens.end()) {
      current = it->second.get();
      start_pos = length; // Advance the position
    } else {
      ++length; // Key not found
    }
  }

  // If string doesn't exist in level 1
  if (length > text.size())
    return nullptr;

  // Traverse down the tree
  while (start_pos < text.size()) {
    // If string end, but still node not found
    if ((start_pos + length) > text.size()) {
      return nullptr;
    }

    // Extract the next substring
    findStr = text.substr(start_pos, length);
    auto childIt = current->childrens.find(findStr);

    // If node matched
    if (childIt != current->childrens.end()) {
      current = childIt->second.get();
      start_pos += length; // Advance position
      length = 1;
    } else {
      ++length;
    }
  }

  if (current->isNull()) {
    return nullptr;
  }

  return current;
}
}