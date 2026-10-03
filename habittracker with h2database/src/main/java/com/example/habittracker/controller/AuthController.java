package com.example.habittracker.controller;

import com.example.habittracker.model.User;
import com.example.habittracker.service.UserService;
import jakarta.servlet.http.HttpSession;
import org.springframework.stereotype.Controller;
import org.springframework.ui.Model;
import org.springframework.web.bind.annotation.*;

@Controller
public class AuthController {

    private final UserService userService;

    public AuthController(UserService userService) {
        this.userService = userService;
    }

    // Show registration page
    @GetMapping("/register")
    public String showRegisterPage(Model model) {

        model.addAttribute("user", new User());

        return "register";
    }

    // Handle registration
    @PostMapping("/register")
    public String registerUser(
            @ModelAttribute("user") User user,
            Model model) {

        User existingUser = userService.findByEmail(user.getEmail());

        if (existingUser != null) {
            model.addAttribute("error", "Email already exists!");
            return "register";
        }

        userService.registerUser(user);

        return "redirect:/login";
    }

    // Show login page
    @GetMapping("/login")
    public String showLoginPage() {

        return "login";
    }

    // Handle login
    @PostMapping("/login")
    public String loginUser(
            @RequestParam String email,
            @RequestParam String password,
            HttpSession session,
            Model model) {

        boolean loginSuccessful =
                userService.loginUser(email, password);

        if (!loginSuccessful) {
            model.addAttribute("error", "Invalid email or password!");
            return "login";
        }

        User user = userService.findByEmail(email);

        // Store logged-in user in session
        session.setAttribute("loggedInUser", user);

        return "redirect:/";
    }

    // Logout
    @GetMapping("/logout")
    public String logout(HttpSession session) {

        session.invalidate();

        return "redirect:/login";
    }
}