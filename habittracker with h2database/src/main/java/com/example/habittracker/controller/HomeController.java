package com.example.habittracker.controller;

import com.example.habittracker.service.CategoryService;
import com.example.habittracker.service.HabitService;
import jakarta.servlet.http.HttpSession;
import org.springframework.stereotype.Controller;
import org.springframework.ui.Model;
import org.springframework.web.bind.annotation.GetMapping;

@Controller
public class HomeController {

    private final HabitService habitService;
    private final CategoryService categoryService;

    public HomeController(HabitService habitService,
                          CategoryService categoryService) {
        this.habitService = habitService;
        this.categoryService = categoryService;
    }

    @GetMapping("/")
    public String dashboard(Model model, HttpSession session) {

        // Check if user is logged in
        if (session.getAttribute("loggedInUser") == null) {
            return "redirect:/login";
        }

        // Dashboard statistics
        model.addAttribute("totalHabits",
                habitService.getTotalHabits());

        model.addAttribute("completedHabits",
                habitService.getCompletedHabits());

        model.addAttribute("pendingHabits",
                habitService.getPendingHabits());

        model.addAttribute("totalCategories",
                categoryService.getTotalCategories());

        return "dashboard";
    }
}