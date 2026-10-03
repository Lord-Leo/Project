package com.example.habittracker.controller;

import com.example.habittracker.model.Habit;
import com.example.habittracker.service.CategoryService;
import com.example.habittracker.service.HabitService;
import jakarta.servlet.http.HttpSession;
import org.springframework.stereotype.Controller;
import org.springframework.ui.Model;
import org.springframework.web.bind.annotation.*;

@Controller
public class HabitController {

    private final HabitService habitService;
    private final CategoryService categoryService;

    public HabitController(HabitService habitService,
                           CategoryService categoryService) {
        this.habitService = habitService;
        this.categoryService = categoryService;
    }

    // Display all habits
    @GetMapping("/habits")
    public String showHabits(Model model, HttpSession session) {

        // Check if user is logged in
        if (session.getAttribute("loggedInUser") == null) {
            return "redirect:/login";
        }

        model.addAttribute("habits",
                habitService.getAllHabits());

        return "habits";
    }

    // Show add habit form
    @GetMapping("/habit/new")
    public String showAddHabitForm(Model model,
                                   HttpSession session) {

        if (session.getAttribute("loggedInUser") == null) {
            return "redirect:/login";
        }

        model.addAttribute("habit", new Habit());

        model.addAttribute("categories",
                categoryService.getAllCategories());

        return "add-habit";
    }

    // Save new habit
    @PostMapping("/habit/save")
    public String saveHabit(@ModelAttribute Habit habit,
                            HttpSession session) {

        if (session.getAttribute("loggedInUser") == null) {
            return "redirect:/login";
        }

        habitService.saveHabit(habit);

        return "redirect:/habits";
    }

    // Show edit habit form
    @GetMapping("/habit/edit/{id}")
    public String showEditHabitForm(@PathVariable Long id,
                                    Model model,
                                    HttpSession session) {

        if (session.getAttribute("loggedInUser") == null) {
            return "redirect:/login";
        }

        Habit habit = habitService.getHabitById(id)
                .orElseThrow(() ->
                        new IllegalArgumentException(
                                "Invalid habit ID: " + id));

        model.addAttribute("habit", habit);

        model.addAttribute("categories",
                categoryService.getAllCategories());

        return "edit-habit";
    }

    // Update habit
    @PostMapping("/habit/update")
    public String updateHabit(@ModelAttribute Habit habit,
                              HttpSession session) {

        if (session.getAttribute("loggedInUser") == null) {
            return "redirect:/login";
        }

        habitService.saveHabit(habit);

        return "redirect:/habits";
    }

    // Mark habit as completed
    @GetMapping("/habit/complete/{id}")
    public String completeHabit(@PathVariable Long id,
                                HttpSession session) {

        if (session.getAttribute("loggedInUser") == null) {
            return "redirect:/login";
        }

        habitService.markAsCompleted(id);

        return "redirect:/habits";
    }

    // Delete habit
    @GetMapping("/habit/delete/{id}")
    public String deleteHabit(@PathVariable Long id,
                              HttpSession session) {

        if (session.getAttribute("loggedInUser") == null) {
            return "redirect:/login";
        }

        habitService.deleteHabit(id);

        return "redirect:/habits";
    }
}