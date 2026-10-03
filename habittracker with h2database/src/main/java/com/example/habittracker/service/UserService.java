package com.example.habittracker.service;

import com.example.habittracker.model.User;
import com.example.habittracker.repository.UserRepository;
import org.springframework.stereotype.Service;

@Service
public class UserService {

    private final UserRepository userRepository;

    public UserService(UserRepository userRepository) {
        this.userRepository = userRepository;
    }

    // Register a new user
    public User registerUser(User user) {

        // Check if email already exists
        if (userRepository.findByEmail(user.getEmail()).isPresent()) {
            return null;
        }

        return userRepository.save(user);
    }

    // Find user by email
    public User findByEmail(String email) {
        return userRepository.findByEmail(email).orElse(null);
    }

    // Check login credentials
    public boolean loginUser(String email, String password) {

        User user = findByEmail(email);

        if (user == null) {
            return false;
        }

        return user.getPassword().equals(password);
    }
}