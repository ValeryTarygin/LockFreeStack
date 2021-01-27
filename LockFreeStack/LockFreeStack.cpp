//-------------------------------------------------------------------------------------------------
#include <iostream>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>
//-------------------------------------------------------------------------------------------------
template<typename T>
class lock_free_stack
{
	struct node
	{
		std::shared_ptr<T> _data;
		node* _next;
		node(T const& data_) :
			_data(std::make_shared<T>(data_))
		{}
	};
	std::atomic<node*> head_;
	std::atomic<unsigned> threads_in_pop_;
	std::atomic<node*> to_be_deleted_;

	static void delete_nodes(node* nodes)
	{
		while(nodes)
		{
			node* next = nodes->_next;
			delete nodes;
			nodes = next;
		}
	}
	void try_reclaim(node* old_head)
	{
		if(threads_in_pop_ == 1)
		{
			node* nodes_to_delete = to_be_deleted_.exchange(nullptr);
			if(!--threads_in_pop_)
			{
				delete_nodes(nodes_to_delete);
			}
			else if(nodes_to_delete)
			{
				chain_pending_nodes(nodes_to_delete);
			}
			delete old_head;
		}
		else
		{
			chain_pending_node(old_head);
			--threads_in_pop_;
		}
	}
	void chain_pending_nodes(node* nodes)
	{
		node* last = nodes;
		while(node* const next = last->_next)
		{
			last = next;
		}
		chain_pending_nodes(nodes, last);
	}

	void chain_pending_nodes(node* first, node* last)
	{
		last->_next = to_be_deleted_;
		while(!to_be_deleted_.compare_exchange_weak(last->_next, first));
	}

	void chain_pending_node(node* n)
	{
		chain_pending_nodes(n, n);
	}

public:
	void push(T const& data)
	{
		node* const new_node = new node(data);
		new_node->_next = head_.load();
		while(!head_.compare_exchange_weak(new_node->_next, new_node));
	}

	std::shared_ptr<T> pop()
	{
		++threads_in_pop_;
		node* old_head = head_.load();
		while(old_head &&
			!head_.compare_exchange_weak(old_head, old_head->_next));
		std::shared_ptr<T> res;
		if(old_head)
		{
			res.swap(old_head->_data);
		}
		try_reclaim(old_head);
		return res;
	}
};

int main()
{
	int flagExit;
	lock_free_stack<int> stack;
	std::vector<std::thread> vThread;

	vThread.emplace_back(std::thread([&flagExit, &stack]() {
		int newVal;
		while(true)
		{
			std::cin >> newVal;
			flagExit = newVal;
			if(flagExit == 10)
				break;

			stack.push(newVal);
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
		}));


	vThread.emplace_back(std::thread([&flagExit, &stack]() {
		while(true)
		{
			if(flagExit == 10)
				break;

			if(auto shareVal = stack.pop())
				std::cout << *shareVal << std::endl;

			std::this_thread::sleep_for(std::chrono::milliseconds(5000));
		}
		}));

	for(auto& it : vThread)
		it.join();

	system("pause");
	return 0;
}
//-------------------------------------------------------------------------------------------------