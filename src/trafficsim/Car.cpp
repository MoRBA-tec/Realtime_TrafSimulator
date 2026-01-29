#include "Car.hpp"
#include "util/VectorMath.hpp" //

#include <iostream>
#include <algorithm> // std::max
#include "Rando.hpp"


namespace ts
{

std::vector<const sf::Texture *> Car::Textures_;
unsigned int Car::accident_count_ = 0;

Car::Car(const std::shared_ptr<Node> &pos, const std::shared_ptr<Node> &dest, const sf::Vector2f &size)
    : pos_(pos), dest_(dest), prev_(pos), shape_(size), speed_(200), acceleration_(200)
{
    Rando r(Textures_.size());
    int r_i = r.uniroll();
    shape_.setTexture(Textures_.at(r_i - 1));
    shape_.setOrigin({shape_.getSize().x / 2, shape_.getSize().y / 2});
    shape_.setPosition(pos_->getPos());
    findRoute();
}

void Car::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
    target.draw(shape_, states);
}

void Car::AddTexture(const sf::Texture *carTexture)
{
    Textures_.push_back(carTexture);
}

void Car::update(const sf::Time &game_time, float deltatime, const std::vector<std::unique_ptr<Car>> &cars, const std::map<unsigned int, std::unique_ptr<TrafficLightNetwork>> &light_handlers)
{
    if (route_.size() < 1)
    {
        finished = true;
        return;
    }

    // Prüfe auf mögliche Unfälle
    if (!is_in_accident_ && checkAccident(cars))
    {
        handleAccident();
    }

// Neu--------------------------------------------------------------------------------------------------------------------
// Pruft ob Vorfahrt
    bool blockiert = false;
    for (const auto& otherCar : cars)
    {
        if (otherCar.get() == this) continue;

        if (!hasRightOfWay(*otherCar))
        {
            blockiert = true;
            break;
        }
    }
//------------------------------------------------------------------------------------------------------------------------

// Neu--------------------------------------------------------------------------------------------------------------------
    if (blockiert)
    {
        if (wait_timer_.getElapsedTime().asSeconds() < 2.0f)
        {
            return; // warten
        }

        wait_timer_.restart(); // nach warten zurücksetzten
    }
    else
    {
        wait_timer_.restart(); // kein warten -> zurücksetzen
    }
//------------------------------------------------------------------------------------------------------------------------


    float delta_step = deltatime * speed_;
    if (VectorMath::Distance(shape_.getPosition(), route_.front()->getPos()) < delta_step)
    {
        // Increment node cars_passed counter
        prev_->incrementCounter(game_time);
        shape_.setPosition(route_.front()->getPos());
        prev_ = route_.front();
        route_.pop_front();
        if (route_.size() < 1 || VectorMath::Distance(dest_->getPos(), route_.front()->getPos()) < delta_step)
        {
            finished = true;
            return;
        }
        // We only need to change rotation of the car on turn
        dir_ = VectorMath::Normalize(route_.front()->getPos() - prev_->getPos());
        if (dir_.x == 1)
            shape_.setRotation(90);
        else if (dir_.x == -1)
            shape_.setRotation(270);
        else if (dir_.y == -1)
            shape_.setRotation(0);
        else if (dir_.y == 1)
            shape_.setRotation(180);
    }
    // Checks if there is something (another car) infront of this car
    // if there is -> we cant move forward
    calculateVelocity(deltatime, cars, light_handlers);
    shape_.move(dir_ * deltatime * speed_);
}

void Car::calculateVelocity(float deltatime, const std::vector<std::unique_ptr<Car>> &cars, const std::map<unsigned int, std::unique_ptr<TrafficLightNetwork>> &light_handlers)
{
    for (const auto &car : cars)
    {
        // safe distance to the car infront
        if (car->shape_.getGlobalBounds().contains(shape_.getPosition() + dir_ * shape_.getSize().y))
        {
            speed_ = 0;
            return;
        }
        // car front
        if (car->shape_.getGlobalBounds().contains(shape_.getPosition() + dir_ * shape_.getSize().y * 0.51f))
        {
            speed_ = 0;
            return;
        }
    }
    for (auto ita = light_handlers.begin(); ita != light_handlers.end(); ++ita)
    {
        const auto &lights = ita->second->getLights();
        //sf::FloatRect rect(shape_.getPosition() - shape_.getOrigin(), {std::max(dir_.x * shape_.getSize().y*5.f, 2.f), std::max(dir_.y * shape_.getSize().y*5.f, 2.f)});
        for (const auto &light : lights)
        {
            if (!light->canDrive())
            {
                // if (light->getBlocker().getGlobalBounds().intersects(rect))
                // {
                //     speed_ = std::max(speed_ - acceleration_ * acceleration_ * deltatime, 0.f);
                //     return;
                // }
                // // car front
                // if (light->getBlocker().getGlobalBounds().intersects(rect))
                // {
                //     speed_ = std::max(speed_ - acceleration_ * acceleration_* deltatime, 0.f);
                //     return;
                // }
                if (light->getBlocker().getGlobalBounds().contains(shape_.getPosition() + dir_ * shape_.getSize().y))
                {
                    speed_ = 0;
                    return;
                }
                // car front
                if (light->getBlocker().getGlobalBounds().contains(shape_.getPosition() + dir_ * shape_.getSize().y * 0.51f))
                {
                    speed_ = 0;
                    return;
                }
            }
            // light infront
        }
    }
    speed_ = std::min(speed_ + acceleration_ * deltatime, 200.f);



// Neu--------------------------------------------------------------------------------------------------------------------
// Neu Zielknoten bereits belegt?
    if (!route_.empty())
    {
        const auto& myNext = route_.front();
        for (const auto& car : cars)
        {
            if (car.get() == this) continue;

            if (!car->route_.empty())
            {
                auto otherPos = car->shape_.getPosition();
                auto nodePos = myNext->getPos();

                float distance = VectorMath::Distance(otherPos, nodePos);
                if (distance < 80.f)
                {
                    speed_ = std::max(speed_ - acceleration_ * deltatime * 1.5f, 0.f);
                    return;
                }
            }
        }
    }
//------------------------------------------------------------------------------------------------------------------------

// Neu--------------------------------------------------------------------------------------------------------------------
// Neu Vorfahrt beachten
    for (const auto& car : cars)
    {
        if (car.get() == this) continue;

        if (!hasRightOfWay(*car))
        {
            speed_ = std::max(speed_ - acceleration_ * deltatime, 0.f);
            return;
        }
    }
//------------------------------------------------------------------------------------------------------------------------

// Neu--------------------------------------------------------------------------------------------------------------------
// Neu beide Autos wollen zur gleichen Kreuzung?
    for (const auto &car : cars)
    {
        if (car.get() == this) continue;

        if (!route_.empty() && !car->route_.empty())
        {
            auto myNext = route_.front();
            auto otherNext = car->route_.front();

            if (myNext == otherNext)
            {
                float dist = VectorMath::Distance(car->shape_.getPosition(), myNext->getPos());
                if (dist < 80.f)
                {
                    speed_ = 0;
                    return;
                }
            }
        }
    }

// Neu Kollision mit Autos direkt vor vermeiden
    for (const auto &car : cars)
    {
        if (car->shape_.getGlobalBounds().contains(shape_.getPosition() + dir_ * shape_.getSize().y) ||
            car->shape_.getGlobalBounds().contains(shape_.getPosition() + dir_ * shape_.getSize().y * 0.51f))
        {
            speed_ = 0;
            return;
        }
    }

//  Ampeln beachten
    for (auto ita = light_handlers.begin(); ita != light_handlers.end(); ++ita)
    {
        const auto &lights = ita->second->getLights();

        for (const auto &light : lights)
        {
            if (!light->canDrive())
            {
                if (light->getBlocker().getGlobalBounds().contains(shape_.getPosition() + dir_ * shape_.getSize().y) ||
                    light->getBlocker().getGlobalBounds().contains(shape_.getPosition() + dir_ * shape_.getSize().y * 0.51f))
                {
                    speed_ = 0;
                    return;
                }
            }
        }
    }

// Neu Alles frei – beschleunigen
    speed_ = std::min(speed_ + acceleration_ * deltatime, 200.f);
//------------------------------------------------------------------------------------------------------------------------
}





void Car::findRoute()
{
    std::map<std::shared_ptr<Node>, bool> visited;
    Node::search_AStar(pos_, dest_, visited, route_);
}

bool Car::checkAccident(const std::vector<std::unique_ptr<Car>> &cars)
{
    const float MIN_SAFE_DISTANCE = shape_.getSize().y * 0.05f; // 10% der Fahrzeuglänge als Mindestabstand
    
    for (const auto &car : cars)
    {
        if (car.get() == this) continue; // Skip own car
        
        float distance = VectorMath::Distance(shape_.getPosition(), car->shape_.getPosition());
        if (distance < MIN_SAFE_DISTANCE)
        {
            // Check if the cars are driving in the same direction
            if (VectorMath::Dot(dir_, car->dir_) > 0.8f) // If the direction vectors are almost parallel
            {
                return true;
            }
        }
    }
    return false;
}


void Car::handleAccident()
{
    is_in_accident_ = true;
    speed_ = 0;
    accident_count_++; // Increase accident counter
    createAccidentGraphics();
    
    // Block the current road section
    if (prev_ && route_.size() > 0)
    {
        // Mark the current road section as blocked
        prev_->setBlocked(true);
        route_.front()->setBlocked(true);
        
        // Search for alternative route
        findRoute();
    }
}

// Creates a larger red rectangle to indicate an accident
// This rectangle is drawn instead of the normal car shape when in an accident
void Car::createAccidentGraphics()
{
    accident_shape_.setSize(shape_.getSize() * 1.5f);
    accident_shape_.setPosition(shape_.getPosition());
    accident_shape_.setFillColor(sf::Color::Red);
    accident_shape_.setOrigin(accident_shape_.getSize() / 2.f);
}


// Draws the car or the accident shape depending on whether the car is in an accident
// If the car is in an accident, it draws the accident shape instead of the normal car
void Car::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
    if (is_in_accident_)
    {
        target.draw(accident_shape_, states);
    } else
    {
        target.draw(shape_, states);
    }
    // Draw the accident shape if in accident
}


// Neu--------------------------------------------------------------------------------------------------------------------
// Richtung für Vorfahrt
int directionToIndex(const sf::Vector2f& dir)
{
    if (std::fabs(dir.x - 1) < 0.5 && std::fabs(dir.y) < 0.5) return 0; // rechts
    if (std::fabs(dir.y - 1) < 0.5 && std::fabs(dir.x) < 0.5) return 1; // unten
    if (std::fabs(dir.x + 1) < 0.5 && std::fabs(dir.y) < 0.5) return 2; // links
    if (std::fabs(dir.y + 1) < 0.5 && std::fabs(dir.x) < 0.5) return 3; // oben
    return -1;
}
//------------------------------------------------------------------------------------------------------------------------

// Neu--------------------------------------------------------------------------------------------------------------------
// Logik Distanz und Richtung
bool Car::hasRightOfWay(const Car& other) const
{
    if (route_.empty() || other.route_.empty()) return true;

    auto myNext = route_.front();
    auto otherNext = other.route_.front();
    if (myNext != otherNext) return true;

    float myDist = VectorMath::Distance(shape_.getPosition(), myNext->getPos());
    float otherDist = VectorMath::Distance(other.shape_.getPosition(), otherNext->getPos());

    if (myDist > 400.f) return true;
    if (otherDist > 400.f) return true;

    if (myDist < otherDist) return true;

    sf::Vector2f myDir = VectorMath::Normalize(myNext->getPos() - shape_.getPosition());
    sf::Vector2f otherDir = VectorMath::Normalize(otherNext->getPos() - other.shape_.getPosition());

    int myIndex = directionToIndex(myDir);
    int otherIndex = directionToIndex(otherDir);

    if (myIndex == -1 || otherIndex == -1) return true;

    // Normalfall: Rechts vor Links
    if ((myIndex + 1) % 4 == otherIndex)
    {
        return false; // Ich muss warten
    }

    // Erweiterung: Ich fahre durch, anderer will einbiegen
    if ((otherIndex + 1) % 4 == myIndex)
    {
        return false;
    }

    return true;
}
//------------------------------------------------------------------------------------------------------------------------

} // namespace ts
