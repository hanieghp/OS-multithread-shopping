# Shopping System Pipeline Documentation

## Overview
This project implements a multi-threaded, multi-process shopping system with user management and rating capabilities. The system uses shared memory, semaphores, and thread synchronization to handle concurrent operations.

## System Architecture

### 1. Data Structure
- **UserShoppingList**: Core data structure containing:
  - User information
  - Budget constraints
  - Product ratings
  - Synchronization primitives

### 2. Process Hierarchy
```
Main Process
└── User Process
    └── Store Processes (1-3)
        └── Category Processes (1-8)
            └── Product Search Threads
```

#### a. User Input Layer
- Graphical interface for user data collection
- Collects:
  - User ID
  - Product list
  - Quantities
  - Budget cap
- Data validation and input processing

#### b. Process Management Layer
- Creates user processes
- Manages shared memory allocation
- Handles process synchronization
- Implements store and category process creation

#### c. Search Layer
- Multi-threaded product search across stores
- File system traversal
- Product matching and validation
- Real-time logging of search operations

#### d. Analysis Layer
- Basket value calculation
- Store comparison
- Budget validation
- Previous purchase history check for discounts

#### e. Transaction Layer
- Updates product inventory
- Handles concurrent access
- Maintains transaction logs
- Updates product ratings

## Synchronization Mechanisms
1. Semaphores:
   - Product search (`SEM_PRODUCT_SEARCH`)
   - Result updates (`SEM_RESULT_UPDATE`)
   - Rating updates (`SEM_RATING_UPDATE`)
   - Shopping list access (`SEM_SHOPPING_LIST`)

2. Mutexes:
   - Thread state control
   - Shared resource access
   - Process coordination

## Areas for Improvement

### 1. Performance Optimization
- [ ] Implement caching for frequently accessed products
- [ ] Optimize file I/O operations
- [ ] Add database integration for better data management
- [ ] Implement connection pooling

### 2. Scalability
- [ ] Add support for distributed systems
- [ ] Implement load balancing
- [ ] Add horizontal scaling capabilities
- [ ] Create microservices architecture

### 3. Feature Additions
- [ ] Add user authentication and authorization
- [ ] Implement real-time inventory tracking
- [ ] Add payment processing system
- [ ] Create recommendation engine
- [ ] Add shopping history tracking
- [ ] Implement wishlist functionality

### 4. User Interface
- [ ] Add responsive web interface
- [ ] Implement mobile app version
- [ ] Add admin dashboard
- [ ] Improve user experience with better feedback
- [ ] Add search filters and sorting options

### 5. Security
- [ ] Implement encryption for sensitive data
- [ ] Add input validation and sanitization
- [ ] Implement secure session management
- [ ] Add rate limiting
- [ ] Implement audit logging

### 6. Error Handling
- [ ] Add comprehensive error logging
- [ ] Implement retry mechanisms
- [ ] Add transaction rollback capability
- [ ] Improve error reporting
- [ ] Add system monitoring

### 7. Testing
- [ ] Add unit tests
- [ ] Implement integration tests
- [ ] Add load testing
- [ ] Implement continuous integration
- [ ] Add automated deployment

## File Structure
```
project/
├── src/
│   ├── main.c
│   ├── user/
│   │   └── user_management.c
│   ├── store/
│   │   └── store_operations.c
│   └── utils/
│       └── helpers.c
├── include/
│   └── headers/
├── tests/
├── docs/
└── README.md
```

## Dependencies
- raylib
- POSIX threads
- System V IPC
- Standard C libraries

## Building and Running
1. Install required dependencies
2. Compile the project using make
3. Run the executable
4. Follow the GUI prompts for shopping

## Future Considerations
1. Consider moving to a client-server architecture
2. Implement RESTful API
3. Add support for multiple currencies
4. Implement internationalization
5. Add analytics and reporting features

## Contributing
1. Fork the repository
2. Create a feature branch
3. Submit a pull request
4. Follow coding standards

##abstraction :
we have a syste, that at first give input from user contains : budgetCap , items, user id and for handling that at forst 
we want to search for that items :
1. forking for stors
2. forking for categories
3. threading for items
at the end :
1. a thread for calculating the best store(basketThread function)
2. a thread for calculating rates and waking up the threads that have found them and tell them to update that(ratingThread)
3. a thread for calculating entities and waking up the threads that have found them and tell them to update that(entityThread)
